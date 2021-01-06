/**
 *    Copyright (C) 2016 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/db/s/balancer/migration_manager.h"

#include <memory>

#include "mongo/bson/simple_bsonobj_comparator.h"
#include "mongo/bson/util/bson_extract.h"
#include "mongo/client/remote_command_targeter.h"
#include "mongo/db/client.h"
#include "mongo/db/repl/repl_set_config.h"
#include "mongo/db/repl/replication_coordinator.h"
#include "mongo/db/s/balancer/scoped_migration_request.h"
#include "mongo/db/s/balancer/type_migration.h"
#include "mongo/executor/task_executor_pool.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/catalog_cache.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/s/move_chunk_request.h"
#include "mongo/util/log.h"
#include "mongo/util/net/hostandport.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

using executor::RemoteCommandRequest;
using executor::RemoteCommandResponse;
using std::shared_ptr;
using std::vector;
using str::stream;

namespace {

const char kChunkTooBig[] = "chunkTooBig";  // TODO: delete in 3.8
const WriteConcernOptions kMajorityWriteConcern(WriteConcernOptions::kMajority,
                                                WriteConcernOptions::SyncMode::UNSET,
                                                Seconds(15));

/**
 * Parses the 'commandResponse' and converts it to a status to use as the outcome of the command.
 * Preserves backwards compatibility with 3.4 and earlier shards that, rather than use a ChunkTooBig
 * error code, include an extra field in the response.
 *
 * TODO: Delete in 3.8
 */
Status extractMigrationStatusFromCommandResponse(const BSONObj& commandResponse) {
    Status commandStatus = getStatusFromCommandResult(commandResponse);

    if (!commandStatus.isOK()) {
        bool chunkTooBig = false;
        bsonExtractBooleanFieldWithDefault(commandResponse, kChunkTooBig, false, &chunkTooBig)
            .transitional_ignore();
        if (chunkTooBig) {
            commandStatus = {ErrorCodes::ChunkTooBig, commandStatus.reason()};
        }
    }

    return commandStatus;
}


/**
 * Returns whether the specified status is an error caused by stepdown of the primary config node
 * currently running the balancer.
 */
bool isErrorDueToConfigStepdown(Status status, bool isStopping) {
    return ((status == ErrorCodes::CallbackCanceled && isStopping) ||
            status == ErrorCodes::BalancerInterrupted ||
            status == ErrorCodes::InterruptedDueToReplStateChange);
}

}  // namespace

MigrationManager::MigrationManager(ServiceContext* serviceContext)
    : _serviceContext(serviceContext) {}

MigrationManager::~MigrationManager() {
    // The migration manager must be completely quiesced at destruction time
    invariant(_activeMigrations.empty());
}

//源分片收到config server发送过来的moveChunk命令  
//注意MoveChunkCmd和MoveChunkCommand的区别，MoveChunkCmd为代理收到mongo shell等客户端的处理流程，
//然后调用configsvr_client::moveChunk，发送_configsvrMoveChunk给config server,由config server统一
//发送movechunk给shard执行chunk操作，从而执行MoveChunkCommand::run来完成shard见真正的shard间迁移

//MoveChunkCommand为shard收到movechunk命令的真正数据迁移的入口
//MoveChunkCmd为mongos收到客户端movechunk命令的处理流程，转发给config server
//ConfigSvrMoveChunkCommand为config server收到mongos发送来的_configsvrMoveChunk命令的处理流程

//自动balancer触发shard做真正的数据迁移入口在Balancer::_moveChunks->MigrationManager::executeMigrationsForAutoBalance
//手动balance，config收到代理ConfigSvrMoveChunkCommand命令后迁移入口Balancer::moveSingleChunk

//Balancer::_moveChunks调用
MigrationStatuses MigrationManager::executeMigrationsForAutoBalance(
    OperationContext* opCtx,
    //需要迁移的块 
    const vector<MigrateInfo>& migrateInfos,
    uint64_t maxChunkSizeBytes,
    const MigrationSecondaryThrottleOptions& secondaryThrottle,
    bool waitForDelete) {

	//balance的chunk迁移状态信息记录到这里面
    MigrationStatuses migrationStatuses;

    {	//
        std::map<MigrationIdentifier, ScopedMigrationRequest> scopedMigrationRequests;
        vector<std::pair<shared_ptr<Notification<RemoteCommandResponse>>, MigrateInfo>> responses;

		//遍历需要迁移的chunk信息
        for (const auto& migrateInfo : migrateInfos) {
            // Write a document to the config.migrations collection, in case this migration must be
            // recovered by the Balancer. Fail if the chunk is already moving.
           // 写需要迁移的chunk信息到config.migrations表，如果有冲突返回失败
            auto statusWithScopedMigrationRequest = //ScopedMigrationRequest类型
                ScopedMigrationRequest::writeMigration(opCtx, migrateInfo, waitForDelete);
            if (!statusWithScopedMigrationRequest.isOK()) {
				//该chunk写表config.migrations异常，记录下来，说明该chunk迁移信息异常
                migrationStatuses.emplace(migrateInfo.getName(),
                                          std::move(statusWithScopedMigrationRequest.getStatus()));
                continue;
            }
            scopedMigrationRequests.emplace(migrateInfo.getName(),
                                            std::move(statusWithScopedMigrationRequest.getValue()));

			//向源分片发送moveChunk命令，同时获取返回结果存入responses
            responses.emplace_back(
                _schedule(opCtx, migrateInfo, maxChunkSizeBytes, secondaryThrottle, waitForDelete),
                migrateInfo);
        }

        // Wait for all the scheduled migrations to complete.
        //获取对应responese，等待本循环中这一批migrateInfos迁移完成
        for (auto& response : responses) {
            auto notification = std::move(response.first);
            auto migrateInfo = std::move(response.second);

            const auto& remoteCommandResponse = notification->get();

            auto it = scopedMigrationRequests.find(migrateInfo.getName());
            invariant(it != scopedMigrationRequests.end());
			//获取moveChunk的结果
            Status commandStatus =
                _processRemoteCommandResponse(remoteCommandResponse, &it->second);
            migrationStatuses.emplace(migrateInfo.getName(), std::move(commandStatus));
        }
    }

    invariant(migrationStatuses.size() == migrateInfos.size());

    return migrationStatuses;
}

//Balancer::moveSingleChunk  Balancer::rebalanceSingleChunk调用
Status MigrationManager::executeManualMigration(
    OperationContext* opCtx,
    const MigrateInfo& migrateInfo,
    uint64_t maxChunkSizeBytes,
    const MigrationSecondaryThrottleOptions& secondaryThrottle,
    bool waitForDelete) {
    _waitForRecovery();

    // Write a document to the config.migrations collection, in case this migration must be
    // recovered by the Balancer. Fail if the chunk is already moving.
    auto statusWithScopedMigrationRequest =
        ScopedMigrationRequest::writeMigration(opCtx, migrateInfo, waitForDelete);
    if (!statusWithScopedMigrationRequest.isOK()) {
        return statusWithScopedMigrationRequest.getStatus();
    }

    RemoteCommandResponse remoteCommandResponse =
        _schedule(opCtx, migrateInfo, maxChunkSizeBytes, secondaryThrottle, waitForDelete)->get();

    auto routingInfoStatus =
        Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(
            opCtx, migrateInfo.ns);
    if (!routingInfoStatus.isOK()) {
        return routingInfoStatus.getStatus();
    }

    auto& routingInfo = routingInfoStatus.getValue();

    auto chunk = routingInfo.cm()->findIntersectingChunkWithSimpleCollation(migrateInfo.minKey);
    invariant(chunk);

    Status commandStatus = _processRemoteCommandResponse(
        remoteCommandResponse, &statusWithScopedMigrationRequest.getValue());

    // Migration calls can be interrupted after the metadata is committed but before the command
    // finishes the waitForDelete stage. Any failovers, therefore, must always cause the moveChunk
    // command to be retried so as to assure that the waitForDelete promise of a successful command
    // has been fulfilled.
    if (chunk->getShardId() == migrateInfo.to && commandStatus != ErrorCodes::BalancerInterrupted) {
        return Status::OK();
    }

    return commandStatus;
}

//Balancer::initiateBalancer调用
void MigrationManager::startRecoveryAndAcquireDistLocks(OperationContext* opCtx) {
    {
        stdx::lock_guard<stdx::mutex> lock(_mutex);
        invariant(_state == State::kStopped);
        invariant(_migrationRecoveryMap.empty());
        _state = State::kRecovering;
    }

    auto scopedGuard = MakeGuard([&] {
        _migrationRecoveryMap.clear();
        _abandonActiveMigrationsAndEnableManager(opCtx);
    });

    auto distLockManager = Grid::get(opCtx)->catalogClient()->getDistLockManager();

    // Load the active migrations from the config.migrations collection.
    auto statusWithMigrationsQueryResponse =
        Grid::get(opCtx)->shardRegistry()->getConfigShard()->exhaustiveFindOnConfig(
            opCtx,
            ReadPreferenceSetting{ReadPreference::PrimaryOnly},
            repl::ReadConcernLevel::kLocalReadConcern,
            NamespaceString(MigrationType::ConfigNS),
            BSONObj(),
            BSONObj(),
            boost::none);

    if (!statusWithMigrationsQueryResponse.isOK()) {
        log() << "Unable to read config.migrations collection documents for balancer migration"
              << " recovery. Abandoning balancer recovery."
              << causedBy(redact(statusWithMigrationsQueryResponse.getStatus()));
        return;
    }

    for (const BSONObj& migration : statusWithMigrationsQueryResponse.getValue().docs) {
        auto statusWithMigrationType = MigrationType::fromBSON(migration);
        if (!statusWithMigrationType.isOK()) {
            // The format of this migration document is incorrect. The balancer holds a distlock for
            // this migration, but without parsing the migration document we cannot identify which
            // distlock must be released. So we must release all distlocks.
            log() << "Unable to parse config.migrations document '" << redact(migration.toString())
                  << "' for balancer migration recovery. Abandoning balancer recovery."
                  << causedBy(redact(statusWithMigrationType.getStatus()));
            return;
        }
        MigrationType migrateType = std::move(statusWithMigrationType.getValue());

        auto it = _migrationRecoveryMap.find(NamespaceString(migrateType.getNss()));
        if (it == _migrationRecoveryMap.end()) {
            std::list<MigrationType> list;
            it = _migrationRecoveryMap.insert(std::make_pair(migrateType.getNss(), list)).first;

            // Reacquire the matching distributed lock for this namespace.
            const std::string whyMessage(stream() << "Migrating chunk(s) in collection "
                                                  << migrateType.getNss().ns());

            auto statusWithDistLockHandle = distLockManager->tryLockWithLocalWriteConcern(
                opCtx, migrateType.getNss().ns(), whyMessage, _lockSessionID);
            if (!statusWithDistLockHandle.isOK()) {
                log() << "Failed to acquire distributed lock for collection '"
                      << migrateType.getNss().ns()
                      << "' during balancer recovery of an active migration. Abandoning"
                      << " balancer recovery."
                      << causedBy(redact(statusWithDistLockHandle.getStatus()));
                return;
            }
        }

        it->second.push_back(std::move(migrateType));
    }

    scopedGuard.Dismiss();
}


//Balancer::_mainThread调用
void MigrationManager::finishRecovery(OperationContext* opCtx,
                                      uint64_t maxChunkSizeBytes,
                                      const MigrationSecondaryThrottleOptions& secondaryThrottle) {
    {
        stdx::lock_guard<stdx::mutex> lock(_mutex);
        if (_state == State::kStopping) {
            _migrationRecoveryMap.clear();
            return;
        }

        // If recovery was abandoned in startRecovery, then there is no more to do.
        if (_state == State::kEnabled) {
            invariant(_migrationRecoveryMap.empty());
            return;
        }

        invariant(_state == State::kRecovering);
    }

    auto scopedGuard = MakeGuard([&] {
        _migrationRecoveryMap.clear();
        _abandonActiveMigrationsAndEnableManager(opCtx);
    });

    // Schedule recovered migrations.
    vector<ScopedMigrationRequest> scopedMigrationRequests;
    vector<shared_ptr<Notification<RemoteCommandResponse>>> responses;

    for (auto& nssAndMigrateInfos : _migrationRecoveryMap) {
        auto& nss = nssAndMigrateInfos.first;
        auto& migrateInfos = nssAndMigrateInfos.second;
        invariant(!migrateInfos.empty());

        auto routingInfoStatus =
            Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx,
                                                                                         nss);
        if (!routingInfoStatus.isOK()) {
            // This shouldn't happen because the collection was intact and sharded when the previous
            // config primary was active and the dist locks have been held by the balancer
            // throughout. Abort migration recovery.
            log() << "Unable to reload chunk metadata for collection '" << nss
                  << "' during balancer recovery. Abandoning recovery."
                  << causedBy(redact(routingInfoStatus.getStatus()));
            return;
        }

        auto& routingInfo = routingInfoStatus.getValue();

        int scheduledMigrations = 0;

        while (!migrateInfos.empty()) {
            auto migrationType = std::move(migrateInfos.front());
            const auto migrationInfo = migrationType.toMigrateInfo();
            auto waitForDelete = migrationType.getWaitForDelete();
            migrateInfos.pop_front();

            auto chunk =
                routingInfo.cm()->findIntersectingChunkWithSimpleCollation(migrationInfo.minKey);
            invariant(chunk);

            if (chunk->getShardId() != migrationInfo.from) {
                // Chunk is no longer on the source shard specified by this migration. Erase the
                // migration recovery document associated with it.
                ScopedMigrationRequest::createForRecovery(opCtx, nss, migrationInfo.minKey);
                continue;
            }

            scopedMigrationRequests.emplace_back(
                ScopedMigrationRequest::createForRecovery(opCtx, nss, migrationInfo.minKey));

            scheduledMigrations++;

            responses.emplace_back(_schedule(
                opCtx, migrationInfo, maxChunkSizeBytes, secondaryThrottle, waitForDelete));
        }

        // If no migrations were scheduled for this namespace, free the dist lock
        if (!scheduledMigrations) {
            Grid::get(opCtx)->catalogClient()->getDistLockManager()->unlock(
                opCtx, _lockSessionID, nss.ns());
        }
    }

    _migrationRecoveryMap.clear();
    scopedGuard.Dismiss();

    {
        stdx::lock_guard<stdx::mutex> lock(_mutex);
        if (_state == State::kRecovering) {
            _state = State::kEnabled;
            _condVar.notify_all();
        }
    }

    // Wait for each migration to finish, as usual.
    for (auto& response : responses) {
        response->get();
    }
}

//Balancer::interruptBalancer调用，
//_migrationManagerInterruptThread线程专门负责迁移异常处理
void MigrationManager::interruptAndDisableMigrations() {
    executor::TaskExecutor* const executor =
        Grid::get(_serviceContext)->getExecutorPool()->getFixedExecutor();

    stdx::lock_guard<stdx::mutex> lock(_mutex);
    invariant(_state == State::kEnabled || _state == State::kRecovering);
    _state = State::kStopping;

    // Interrupt any active migrations with dist lock
    //对异常balance的表进行异常处理
    for (auto& cmsEntry : _activeMigrations) {
        auto& migrations = cmsEntry.second;

        for (auto& migration : migrations) {
            if (migration.callbackHandle) {
                executor->cancel(*migration.callbackHandle);
            }
        }
    }

    _checkDrained(lock);
}

//Balancer::_mainThread中调用，说明balancer关闭了
void MigrationManager::drainActiveMigrations() {
    stdx::unique_lock<stdx::mutex> lock(_mutex);

    if (_state == State::kStopped)
        return;
    invariant(_state == State::kStopping);
    _condVar.wait(lock, [this] { return _activeMigrations.empty(); });
    _state = State::kStopped;
}

//MigrationManager::executeMigrationsForAutoBalance MigrationManager::finishRecovery
//MigrationManager::executeManualMigration 调用
//向需要迁移的源分片发送moveChunk命令，迁移migrateInfo对应的块
shared_ptr<Notification<RemoteCommandResponse>> 
  MigrationManager::_schedule(
    OperationContext* opCtx,
    const MigrateInfo& migrateInfo,
    uint64_t maxChunkSizeBytes,
    const MigrationSecondaryThrottleOptions& secondaryThrottle,
    bool waitForDelete) {
    const NamespaceString nss(migrateInfo.ns);

    // Ensure we are not stopped in order to avoid doing the extra work
    {
        stdx::lock_guard<stdx::mutex> lock(_mutex);
        if (_state != State::kEnabled && _state != State::kRecovering) {
            return std::make_shared<Notification<RemoteCommandResponse>>(
                Status(ErrorCodes::BalancerInterrupted,
                       "Migration cannot be executed because the balancer is not running"));
        }
    }

	//获取迁移的源分片
    const auto fromShardStatus =
        Grid::get(opCtx)->shardRegistry()->getShard(opCtx, migrateInfo.from);
    if (!fromShardStatus.isOK()) {
        return std::make_shared<Notification<RemoteCommandResponse>>(
            std::move(fromShardStatus.getStatus()));
    }

    const auto fromShard = fromShardStatus.getValue();
	//获取源分片主节点
    auto fromHostStatus = fromShard->getTargeter()->findHost(
        opCtx, ReadPreferenceSetting{ReadPreference::PrimaryOnly});
    if (!fromHostStatus.isOK()) {
        return std::make_shared<Notification<RemoteCommandResponse>>(
            std::move(fromHostStatus.getStatus()));
    }

    BSONObjBuilder builder;
	//生成moveChunk命令内容   moveChunk实际上是发送给源分片的，注意不是发送给cfg的
    MoveChunkRequest::appendAsCommand(
        &builder,
        nss,
        migrateInfo.version,
        repl::ReplicationCoordinator::get(opCtx)->getConfig().getConnectionString(),
        migrateInfo.from,
        migrateInfo.to,
        ChunkRange(migrateInfo.minKey, migrateInfo.maxKey),
        maxChunkSizeBytes,
        secondaryThrottle,
        waitForDelete);

    stdx::lock_guard<stdx::mutex> lock(_mutex);

	//balance disable了
    if (_state != State::kEnabled && _state != State::kRecovering) {
        return std::make_shared<Notification<RemoteCommandResponse>>(
            Status(ErrorCodes::BalancerInterrupted,
                   "Migration cannot be executed because the balancer is not running"));
    }

	//构造Migration， moveChunk命令构造
    Migration migration(nss, builder.obj());

    auto retVal = migration.completionNotification;

    _schedule(lock, opCtx, fromHostStatus.getValue(), std::move(migration));

    return retVal;
}

/*
例如下面的chunks分布:
xx_HTZjQeZL_shard_1 571274
xx_HTZjQeZL_shard_2 319536
xx_HTZjQeZL_shard_3 572644
xx_HTZjQeZL_shard_4 707811
xx_HTZjQeZL_shard_QubQcjup	145339
xx_HTZjQeZL_shard_ewMvmPnE	136034
xx_HTZjQeZL_shard_jaAVvOei	129682
xx_HTZjQeZL_shard_kxYilhNF	150150

这就是选出的需要迁移的chunk
mongos> db.migrations.find()
{ "_id" : "xx_cold_data_db.xx_cold_data_db-user_id_\"359209050\"module_\"album\"md5_\"992F3FF0DDCB009D1A6CCD8647CEAFA5\"", "ns" : "xx_cold_data_db.xx_cold_data_db", "min" : { "user_id" : "359209050", "module" : "album", "md5" : "992F3FF0DDCB009D1A6CCD8647CEAFA5" }, "max" : { "xx" : "359209058", "module" : "album", "md5" : "49D552BEBEAE7D3CB6B53A7FE384E5A4" }, "fromShard" : "xx_HTZjQeZL_shard_1", "toShard" : "xx_HTZjQeZL_shard_QubQcjup", "chunkVersion" : [ Timestamp(588213, 1), ObjectId("5ec496373f311c50a0185499") ], "waitForDelete" : false }
{ "_id" : "xx_cold_data_db.xx_cold_data_db-user_id_\"278344065\"module_\"album\"md5_\"CAA4B99617A83D0DEE5CE30D4D75829F\"", "ns" : "xx_cold_data_db.xx_cold_data_db", "min" : { "user_id" : "278344065", "module" : "album", "md5" : "CAA4B99617A83D0DEE5CE30D4D75829F" }, "max" : { "xx" : "278344356", "module" : "album", "md5" : "E691E5F8756E723BB44BC2049D624F81" }, "fromShard" : "xx_HTZjQeZL_shard_4", "toShard" : "xx_HTZjQeZL_shard_jaAVvOei", "chunkVersion" : [ Timestamp(588211, 1), ObjectId("5ec496373f311c50a0185499") ], "waitForDelete" : false }
{ "_id" : "xx_cold_data_db.xx_cold_data_db-user_id_\"226060685\"module_\"album\"md5_\"56A0293EAD54C7A1326C91621A7C4664\"", "ns" : "xx_cold_data_db.xx_cold_data_db", "min" : { "user_id" : "226060685", "module" : "album", "md5" : "56A0293EAD54C7A1326C91621A7C4664" }, "max" : { "xx" : "226061085", "module" : "album", "md5" : "6686C24B301C39C3B3585D8602A26F45" }, "fromShard" : "xx_HTZjQeZL_shard_3", "toShard" : "xx_HTZjQeZL_shard_ewMvmPnE", "chunkVersion" : [ Timestamp(588212, 1), ObjectId("5ec496373f311c50a0185499") ], "waitForDelete" : false }
*/

//3.6版本balancer migrate由config server触发   

//上面的MigrationManager::_schedule调用
//向源分片主节点发送moveChunk命令    splitchunk也是走这个流程，当发起movechunk如果发现chunk big，则会splite该chunk,也会走到该接口
void MigrationManager::_schedule(WithLock lock,
                                 OperationContext* opCtx,
                                 const HostAndPort& targetHost,
                                 Migration migration) {
    executor::TaskExecutor* const executor =
        Grid::get(opCtx)->getExecutorPool()->getFixedExecutor();

	//获取balance需要迁移的表
    const NamespaceString nss(migration.nss);

	//在map表中查找nss
    auto it = _activeMigrations.find(nss);
	//没找到,说明需要重新获取分布式锁，同一个表多分片情况下，例如从4分片扩容到8分片，则会选择4个需要迁移的chunk
	//在外层的MigrationManager::executeMigrationsForAutoBalance中会每个chunk轮选调用_schedule，这4个chunk实际上只
	//会获取一次分布式锁，也就是第一个chunk进来的时候获取一次分布式锁，后面3个chunk的movechunk操作不会再次获取该分布式锁
	//从而实现了同一个表不同分片扩容的并行迁移
    if (it == _activeMigrations.end()) { //只要当前该表还有chunk在迁移，则不会进入这里面，直接跳过
        const std::string whyMessage(stream() << "Migrating chunk(s) in collection " << nss.ns());

        // Acquire the collection distributed lock (blocking call)
        //获取分布式锁
        auto statusWithDistLockHandle =
        	//ReplSetDistLockManager::lockWithSessionID
            Grid::get(opCtx)->catalogClient()->getDistLockManager()->lockWithSessionID(
                opCtx,
                nss.ns(), //需要做balance的表
                whyMessage,
                _lockSessionID,
                DistLockManager::kSingleLockAttemptTimeout);
		
		//本mongod已经有该nss表的分布式锁，说明当前正在迁移该表数据
        if (!statusWithDistLockHandle.isOK()) { 
            migration.completionNotification->set(
			/*
			类似打印如下:
			Failed with error ‘could not acquire collection lock for mongodbtest.usertable to migrate chunk [{ : MinKey },{ : MaxKey }) 
			:: caused by :: Lock for migrating chunk [{ : MinKey }, { : MaxKey }) in mongodbtest.usertable is taken.’, from shard0002 to shard0000
			*/ //如果chunk很大，则会触发splite，splite也需要获取nss对应分布式锁，这时候就会引起迁移失败
                Status(statusWithDistLockHandle.getStatus().code(),
                       stream() << "Could not acquire collection lock for " << nss.ns()
                                << " to migrate chunks, due to "
                                << statusWithDistLockHandle.getStatus().reason()));
            return;
        }

		//_activeMigrations为CollectionMigrationsStateMap结构
		//记录一下，获取nss表对应锁成功，记录表信息和同时生成一个MigrationsList一起记录MigrationsList
		//MigrationsList在后面的migrations->push_front(std::move(migration))填充
        it = _activeMigrations.insert(std::make_pair(nss, MigrationsList())).first;
    }

	//也就是MigrationsList
    auto migrations = &it->second;

    // Add ourselves to the list of migrations on this collection
    //migration记录到_activeMigrations，这样_activeMigrations中记录的map对为<表，movechunk信息>
    migrations->push_front(std::move(migration));
    auto itMigration = migrations->begin();

	//根据migration构造moveChunk信息发送到targetHost节点 
    const RemoteCommandRequest remoteRequest( 
        targetHost, NamespaceString::kAdminDb.toString(), itMigration->moveChunkCmdObj, opCtx);

    StatusWith<executor::TaskExecutor::CallbackHandle> callbackHandleWithStatus =
        executor->scheduleRemoteCommand(
            remoteRequest,
            [this, itMigration](const executor::TaskExecutor::RemoteCommandCallbackArgs& args) {
                Client::initThread(getThreadName());
                ON_BLOCK_EXIT([&] { Client::destroy(); });
                auto opCtx = cc().makeOperationContext();

                stdx::lock_guard<stdx::mutex> lock(_mutex);
				//该chunk对应movechunk操作执行完成后，调用_complete
                _complete(lock, opCtx.get(), itMigration, args.response);
            });

    if (callbackHandleWithStatus.isOK()) {
		//一般通过这里返回
        itMigration->callbackHandle = std::move(callbackHandleWithStatus.getValue());
        return;
    }

	//正常情况下不会走到这里
	//chunk迁移完成后该接口里面释放分布式锁
    _complete(lock, opCtx, itMigration, std::move(callbackHandleWithStatus.getStatus()));
}

//上面的MigrationManager::_schedule调用
void MigrationManager::_complete(WithLock lock,
                                 OperationContext* opCtx,
                                 MigrationsList::iterator itMigration,
                                 const RemoteCommandResponse& remoteCommandResponse) {
    const NamespaceString nss(itMigration->nss);

    // Make sure to signal the notification last, after the distributed lock is freed, so that we
    // don't have the race condition where a subsequently scheduled migration finds the dist lock
    // still acquired.
    auto notificationToSignal = itMigration->completionNotification;

	//map中查找nss
    auto it = _activeMigrations.find(nss);
    invariant(it != _activeMigrations.end());

	//获取该表nss对应的迁移块信息
    auto migrations = &it->second;
	//移除对应itMigration
    migrations->erase(itMigration);

	//如果该nss下面已经没有itMigration，则释放缓存的分布式锁信息
	//balancer一个循环会选取几个需要迁移的chunk，只有等这批chunk都迁移完成，才会释放分布式锁
    if (migrations->empty()) {
		//DistLockManagerMock::unlock
        Grid::get(opCtx)->catalogClient()->getDistLockManager()->unlock(
            opCtx, _lockSessionID, nss.ns());
        _activeMigrations.erase(it);
        _checkDrained(lock);
    }

    notificationToSignal->set(remoteCommandResponse);
}

void MigrationManager::_checkDrained(WithLock) {
    if (_state == State::kEnabled || _state == State::kRecovering) {
        return;
    }
    invariant(_state == State::kStopping);

    if (_activeMigrations.empty()) {
        _condVar.notify_all();
    }
}

void MigrationManager::_waitForRecovery() {
    stdx::unique_lock<stdx::mutex> lock(_mutex);
    _condVar.wait(lock, [this] { return _state != State::kRecovering; });
}

void MigrationManager::_abandonActiveMigrationsAndEnableManager(OperationContext* opCtx) {
    stdx::unique_lock<stdx::mutex> lock(_mutex);
    if (_state == State::kStopping) {
        // The balancer was interrupted. Let the next balancer recover the state.
        return;
    }
    invariant(_state == State::kRecovering);

    auto catalogClient = Grid::get(opCtx)->catalogClient();

    // Unlock all balancer distlocks we aren't using anymore.
    auto distLockManager = catalogClient->getDistLockManager();
    distLockManager->unlockAll(opCtx, distLockManager->getProcessID());

    // Clear the config.migrations collection so that those chunks can be scheduled for migration
    // again.
    catalogClient
        ->removeConfigDocuments(opCtx, MigrationType::ConfigNS, BSONObj(), kMajorityWriteConcern)
        .transitional_ignore();

    _state = State::kEnabled;
    _condVar.notify_all();
}

//获取moveChunk的结果，MigrationManager::executeMigrationsForAutoBalance调用
Status MigrationManager::_processRemoteCommandResponse(
    const RemoteCommandResponse& remoteCommandResponse,
    ScopedMigrationRequest* scopedMigrationRequest) {

    stdx::lock_guard<stdx::mutex> lock(_mutex);
    Status commandStatus(ErrorCodes::InternalError, "Uninitialized value.");

    // Check for local errors sending the remote command caused by stepdown.
    if (isErrorDueToConfigStepdown(remoteCommandResponse.status,
                                   _state != State::kEnabled && _state != State::kRecovering)) {
        scopedMigrationRequest->keepDocumentOnDestruct();
        return {ErrorCodes::BalancerInterrupted,
                stream() << "Migration interrupted because the balancer is stopping."
                         << " Command status: "
                         << remoteCommandResponse.status.toString()};
    }

    if (!remoteCommandResponse.isOK()) {
        commandStatus = remoteCommandResponse.status;
    } else {
        // TODO: delete in 3.8
        commandStatus = extractMigrationStatusFromCommandResponse(remoteCommandResponse.data);
    }

    if (!Shard::shouldErrorBePropagated(commandStatus.code())) {
        commandStatus = {ErrorCodes::OperationFailed,
                         stream() << "moveChunk command failed on source shard."
                                  << causedBy(commandStatus)};
    }

    // Any failure to remove the migration document should be because the config server is
    // stepping/shutting down. In this case we must fail the moveChunk command with a retryable
    // error so that the caller does not move on to other distlock requiring operations that could
    // fail when the balancer recovers and takes distlocks for migration recovery.
    Status status = scopedMigrationRequest->tryToRemoveMigration();
    if (!status.isOK()) {
        commandStatus = {
            ErrorCodes::BalancerInterrupted,
            stream() << "Migration interrupted because the balancer is stopping"
                     << " and failed to remove the config.migrations document."
                     << " Command status: "
                     << (commandStatus.isOK() ? status.toString() : commandStatus.toString())};
    }

    return commandStatus;
}

MigrationManager::Migration::Migration(NamespaceString inNss, BSONObj inMoveChunkCmdObj)
    : nss(std::move(inNss)),
      moveChunkCmdObj(std::move(inMoveChunkCmdObj)),
      completionNotification(std::make_shared<Notification<RemoteCommandResponse>>()) {}

MigrationManager::Migration::~Migration() {
    invariant(completionNotification);
}

}  // namespace mongo
