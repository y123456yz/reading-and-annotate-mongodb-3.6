/**
 *    Copyright (C) 2017 MongoDB Inc.
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
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kCommand

#include "mongo/platform/basic.h"

#include "mongo/db/service_entry_point_mongod.h"

#include "mongo/base/checked_cast.h"
#include "mongo/db/audit.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/impersonation_session.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/commands/fsync.h"
#include "mongo/db/concurrency/global_lock_acquisition_tracker.h"
#include "mongo/db/curop.h"
#include "mongo/db/curop_metrics.h"
#include "mongo/db/cursor_manager.h"
#include "mongo/db/dbdirectclient.h"
#include "mongo/db/initialize_operation_session_info.h"
#include "mongo/db/introspect.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/lasterror.h"
#include "mongo/db/logical_clock.h"
#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_id_helpers.h"
#include "mongo/db/logical_time_validator.h"
#include "mongo/db/ops/write_ops.h"
#include "mongo/db/ops/write_ops_exec.h"
#include "mongo/db/query/find.h"
#include "mongo/db/read_concern.h"
#include "mongo/db/repl/optime.h"
#include "mongo/db/repl/read_concern_args.h"
#include "mongo/db/repl/repl_client_info.h"
#include "mongo/db/repl/replication_coordinator_global.h"
#include "mongo/db/s/operation_sharding_state.h"
#include "mongo/db/s/sharded_connection_info.h"
#include "mongo/db/s/sharding_state.h"
#include "mongo/db/server_options.h"
#include "mongo/db/session_catalog.h"
#include "mongo/db/stats/counters.h"
#include "mongo/db/stats/top.h"
#include "mongo/rpc/factory.h"
#include "mongo/rpc/metadata.h"
#include "mongo/rpc/metadata/config_server_metadata.h"
#include "mongo/rpc/metadata/logical_time_metadata.h"
#include "mongo/rpc/metadata/oplog_query_metadata.h"
#include "mongo/rpc/metadata/repl_set_metadata.h"
#include "mongo/rpc/metadata/sharding_metadata.h"
#include "mongo/rpc/metadata/tracking_metadata.h"
#include "mongo/rpc/reply_builder_interface.h"
#include "mongo/s/grid.h"
#include "mongo/s/stale_exception.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"
#include "mongo/util/net/message.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

MONGO_FP_DECLARE(rsStopGetMore);
MONGO_FP_DECLARE(respondWithNotPrimaryInCommandDispatch);

namespace {
using logger::LogComponent;

// The command names for which to check out a session.
//
// Note: Eval should check out a session because it defaults to running under a global write lock,
// so if it didn't, and the function it was given contains any of these whitelisted commands, they
// would try to check out a session under a lock, which is not allowed.  Similarly,
// refreshLogicalSessionCacheNow triggers a bulk update under a lock on the sessions collection.
const StringMap<int> cmdWhitelist = {{"delete", 1},
                                     {"eval", 1},
                                     {"$eval", 1},
                                     {"findandmodify", 1},
                                     {"findAndModify", 1},
                                     {"insert", 1},
                                     {"refreshLogicalSessionCacheNow", 1},
                                     {"update", 1}};

void generateLegacyQueryErrorResponse(const AssertionException* exception,
                                      const QueryMessage& queryMessage,
                                      CurOp* curop,
                                      Message* response) {
    curop->debug().exceptionInfo = exception->toStatus();

    log(LogComponent::kQuery) << "assertion " << exception->toString() << " ns:" << queryMessage.ns
                              << " query:" << (queryMessage.query.valid(BSONVersion::kLatest)
                                                   ? queryMessage.query.toString()
                                                   : "query object is corrupt");
    if (queryMessage.ntoskip || queryMessage.ntoreturn) {
        log(LogComponent::kQuery) << " ntoskip:" << queryMessage.ntoskip
                                  << " ntoreturn:" << queryMessage.ntoreturn;
    }

    const StaleConfigException* scex = (exception->code() == ErrorCodes::StaleConfig)
        ? checked_cast<const StaleConfigException*>(exception)
        : NULL;

    BSONObjBuilder err;
    err.append("$err", exception->reason());
    err.append("code", exception->code());
    if (scex) {
        err.append("ok", 0.0);
        err.append("ns", scex->getns());
        scex->getVersionReceived().addToBSON(err, "vReceived");
        scex->getVersionWanted().addToBSON(err, "vWanted");
    }
    BSONObj errObj = err.done();

    if (scex) {
        log(LogComponent::kQuery) << "stale version detected during query over " << queryMessage.ns
                                  << " : " << errObj;
    }

    BufBuilder bb;
    bb.skip(sizeof(QueryResult::Value));
    bb.appendBuf((void*)errObj.objdata(), errObj.objsize());

    // TODO: call replyToQuery() from here instead of this!!! see dbmessage.h
    QueryResult::View msgdata = bb.buf();
    QueryResult::View qr = msgdata;
    qr.setResultFlags(ResultFlag_ErrSet);
    if (scex)
        qr.setResultFlags(qr.getResultFlags() | ResultFlag_ShardConfigStale);
    qr.msgdata().setLen(bb.len());
    qr.msgdata().setOperation(opReply);
    qr.setCursorId(0);
    qr.setStartingFrom(0);
    qr.setNReturned(1);
    response->setData(bb.release());
}

//_generateErrorResponse调用
void registerError(OperationContext* opCtx, const DBException& exception) {
    LastError::get(opCtx->getClient()).setLastError(exception.code(), exception.reason());
    CurOp::get(opCtx)->debug().exceptionInfo = exception.toStatus();
}

void _generateErrorResponse(OperationContext* opCtx,
                            rpc::ReplyBuilderInterface* replyBuilder,
                            const DBException& exception,
                            const BSONObj& replyMetadata) {
    registerError(opCtx, exception);

    // We could have thrown an exception after setting fields in the builder,
    // so we need to reset it to a clean state just to be sure.
    replyBuilder->reset();

    // We need to include some extra information for StaleConfig.
    if (exception.code() == ErrorCodes::StaleConfig) {
        const StaleConfigException& scex = checked_cast<const StaleConfigException&>(exception);
        replyBuilder->setCommandReply(scex.toStatus(),
                                      BSON("ns" << scex.getns() << "vReceived"
                                                << BSONArray(scex.getVersionReceived().toBSON())
                                                << "vWanted"
                                                << BSONArray(scex.getVersionWanted().toBSON())));
    } else {
        replyBuilder->setCommandReply(exception.toStatus());
    }

    replyBuilder->setMetadata(replyMetadata);
}

void _generateErrorResponse(OperationContext* opCtx,
                            rpc::ReplyBuilderInterface* replyBuilder,
                            const DBException& exception,
                            const BSONObj& replyMetadata,
                            LogicalTime operationTime) {
    registerError(opCtx, exception);

    // We could have thrown an exception after setting fields in the builder,
    // so we need to reset it to a clean state just to be sure.
    replyBuilder->reset();

    // We need to include some extra information for StaleConfig.
    if (exception.code() == ErrorCodes::StaleConfig) {
        const StaleConfigException& scex = checked_cast<const StaleConfigException&>(exception);
        replyBuilder->setCommandReply(scex.toStatus(),
                                      BSON("ns" << scex.getns() << "vReceived"
                                                << BSONArray(scex.getVersionReceived().toBSON())
                                                << "vWanted"
                                                << BSONArray(scex.getVersionWanted().toBSON())
                                                << "operationTime"
                                                << operationTime.asTimestamp()));
    } else {
        replyBuilder->setCommandReply(exception.toStatus(),
                                      BSON("operationTime" << operationTime.asTimestamp()));
    }

    replyBuilder->setMetadata(replyMetadata);
}

/**
 * Guard object for making a good-faith effort to enter maintenance mode and leave it when it
 * goes out of scope.
 *
 * Sometimes we cannot set maintenance mode, in which case the call to setMaintenanceMode will
 * return a non-OK status.  This class does not treat that case as an error which means that
 * anybody using it is assuming it is ok to continue execution without maintenance mode.
 *
 * TODO: This assumption needs to be audited and documented, or this behavior should be moved
 * elsewhere.
 */
class MaintenanceModeSetter {
    MONGO_DISALLOW_COPYING(MaintenanceModeSetter);

public:
    MaintenanceModeSetter(OperationContext* opCtx)
        : _opCtx(opCtx),
          _maintenanceModeSet(
              repl::ReplicationCoordinator::get(_opCtx)->setMaintenanceMode(true).isOK()) {}

    ~MaintenanceModeSetter() {
        if (_maintenanceModeSet) {
            repl::ReplicationCoordinator::get(_opCtx)
                ->setMaintenanceMode(false)
                .transitional_ignore();
        }
    }

private:
    OperationContext* const _opCtx;
    const bool _maintenanceModeSet;
};

// Called from the error contexts where request may not be available.
// It only attaches clusterTime and operationTime.
void appendReplyMetadataOnError(OperationContext* opCtx, BSONObjBuilder* metadataBob) {
    auto const replCoord = repl::ReplicationCoordinator::get(opCtx);
    const bool isReplSet =
        replCoord->getReplicationMode() == repl::ReplicationCoordinator::modeReplSet;

    if (isReplSet) {
        if (serverGlobalParams.featureCompatibility.getVersion() ==
            ServerGlobalParams::FeatureCompatibility::Version::kFullyUpgradedTo36) {
            if (LogicalTimeValidator::isAuthorizedToAdvanceClock(opCtx)) {
                // No need to sign cluster times for internal clients.
                SignedLogicalTime currentTime(
                    LogicalClock::get(opCtx)->getClusterTime(), TimeProofService::TimeProof(), 0);
                rpc::LogicalTimeMetadata logicalTimeMetadata(currentTime);
                logicalTimeMetadata.writeToMetadata(metadataBob);
            } else if (auto validator = LogicalTimeValidator::get(opCtx)) {
                auto currentTime =
                    validator->trySignLogicalTime(LogicalClock::get(opCtx)->getClusterTime());
                rpc::LogicalTimeMetadata logicalTimeMetadata(currentTime);
                logicalTimeMetadata.writeToMetadata(metadataBob);
            }
        }
    }
}

void appendReplyMetadata(OperationContext* opCtx,
                         const OpMsgRequest& request,
                         BSONObjBuilder* metadataBob) {
    const bool isShardingAware = ShardingState::get(opCtx)->enabled();
    const bool isConfig = serverGlobalParams.clusterRole == ClusterRole::ConfigServer;
    auto const replCoord = repl::ReplicationCoordinator::get(opCtx);
    const bool isReplSet =
        replCoord->getReplicationMode() == repl::ReplicationCoordinator::modeReplSet;

    if (isReplSet) {
        // Attach our own last opTime.
        repl::OpTime lastOpTimeFromClient =
            repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp();
        replCoord->prepareReplMetadata(opCtx, request.body, lastOpTimeFromClient, metadataBob);
        // For commands from mongos, append some info to help getLastError(w) work.
        // TODO: refactor out of here as part of SERVER-18236
        if (isShardingAware || isConfig) {
            rpc::ShardingMetadata(lastOpTimeFromClient, replCoord->getElectionId())
                .writeToMetadata(metadataBob)
                .transitional_ignore();
        }
        if (serverGlobalParams.featureCompatibility.getVersion() ==
            ServerGlobalParams::FeatureCompatibility::Version::kFullyUpgradedTo36) {
            if (LogicalTimeValidator::isAuthorizedToAdvanceClock(opCtx)) {
                // No need to sign cluster times for internal clients.
                SignedLogicalTime currentTime(
                    LogicalClock::get(opCtx)->getClusterTime(), TimeProofService::TimeProof(), 0);
                rpc::LogicalTimeMetadata logicalTimeMetadata(currentTime);
                logicalTimeMetadata.writeToMetadata(metadataBob);
            } else if (auto validator = LogicalTimeValidator::get(opCtx)) {
                auto currentTime =
                    validator->trySignLogicalTime(LogicalClock::get(opCtx)->getClusterTime());
                rpc::LogicalTimeMetadata logicalTimeMetadata(currentTime);
                logicalTimeMetadata.writeToMetadata(metadataBob);
            }
        }
    }

    // If we're a shard other than the config shard, attach the last configOpTime we know about.
    if (isShardingAware && !isConfig) {
        auto opTime = grid.configOpTime();
        rpc::ConfigServerMetadata(opTime).writeToMetadata(metadataBob);
    }
}

/**
 * Given the specified command and whether it supports read concern, returns an effective read
 * concern which should be used.
 */
StatusWith<repl::ReadConcernArgs> _extractReadConcern(const BSONObj& cmdObj,
                                                      bool supportsNonLocalReadConcern) {
    repl::ReadConcernArgs readConcernArgs;

    auto readConcernParseStatus = readConcernArgs.initialize(cmdObj, Command::testCommandsEnabled);
    if (!readConcernParseStatus.isOK()) {
        return readConcernParseStatus;
    }

    if (!supportsNonLocalReadConcern &&
        readConcernArgs.getLevel() != repl::ReadConcernLevel::kLocalReadConcern) {
        return {ErrorCodes::InvalidOptions,
                str::stream() << "Command does not support non local read concern"};
    }

    return readConcernArgs;
}

void _waitForWriteConcernAndAddToCommandResponse(OperationContext* opCtx,
                                                 const std::string& commandName,
                                                 const repl::OpTime& lastOpBeforeRun,
                                                 BSONObjBuilder* commandResponseBuilder) {
    auto lastOpAfterRun = repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp();

    // Ensures that if we tried to do a write, we wait for write concern, even if that write was
    // a noop.
    if ((lastOpAfterRun == lastOpBeforeRun) &&
        GlobalLockAcquisitionTracker::get(opCtx).getGlobalExclusiveLockTaken()) {
        repl::ReplClientInfo::forClient(opCtx->getClient()).setLastOpToSystemLastOpTime(opCtx);
        lastOpAfterRun = repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp();
    }

    WriteConcernResult res;
    auto waitForWCStatus =
        waitForWriteConcern(opCtx, lastOpAfterRun, opCtx->getWriteConcern(), &res);
    Command::appendCommandWCStatus(*commandResponseBuilder, waitForWCStatus, res);

    // SERVER-22421: This code is to ensure error response backwards compatibility with the
    // user management commands. This can be removed in 3.6.
    if (!waitForWCStatus.isOK() && Command::isUserManagementCommand(commandName)) {
        BSONObj temp = commandResponseBuilder->asTempObj().copy();
        commandResponseBuilder->resetToEmpty();
        Command::appendCommandStatus(*commandResponseBuilder, waitForWCStatus);
        commandResponseBuilder->appendElementsUnique(temp);
    }
}

/**
 * For replica set members it returns the last known op time from opCtx. Otherwise will return
 * uninitialized cluster time.
 */
LogicalTime getClientOperationTime(OperationContext* opCtx) {
    auto const replCoord = repl::ReplicationCoordinator::get(opCtx);
    const bool isReplSet =
        replCoord->getReplicationMode() == repl::ReplicationCoordinator::modeReplSet;
    LogicalTime operationTime;
    if (isReplSet) {
        operationTime = LogicalTime(
            repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp().getTimestamp());
    }
    return operationTime;
}

/**
 * Returns the proper operationTime for a command. To construct the operationTime for replica set
 * members, it uses the last optime in the oplog for writes, last committed optime for majority
 * reads, and the last applied optime for every other read. An uninitialized cluster time is
 * returned for non replica set members.
 */
LogicalTime computeOperationTime(OperationContext* opCtx,
                                 LogicalTime startOperationTime,
                                 repl::ReadConcernLevel level) {
    auto const replCoord = repl::ReplicationCoordinator::get(opCtx);
    const bool isReplSet =
        replCoord->getReplicationMode() == repl::ReplicationCoordinator::modeReplSet;

    if (!isReplSet) {
        return LogicalTime();
    }

    auto operationTime = getClientOperationTime(opCtx);
    invariant(operationTime >= startOperationTime);

    // If the last operationTime has not changed, consider this command a read, and, for replica set
    // members, construct the operationTime with the proper optime for its read concern level.
    if (operationTime == startOperationTime) {
        if (level == repl::ReadConcernLevel::kMajorityReadConcern) {
            operationTime = LogicalTime(replCoord->getLastCommittedOpTime().getTimestamp());
        } else {
            operationTime = LogicalTime(replCoord->getMyLastAppliedOpTime().getTimestamp());
        }
    }

    return operationTime;
}

//runCommands->execCommandDatabase->runCommandImpl调用
bool runCommandImpl(OperationContext* opCtx,
                    Command* command,
                    const OpMsgRequest& request,
                    rpc::ReplyBuilderInterface* replyBuilder,
                    LogicalTime startOperationTime) {
    auto bytesToReserve = command->reserveBytesForReply();

// SERVER-22100: In Windows DEBUG builds, the CRT heap debugging overhead, in conjunction with the
// additional memory pressure introduced by reply buffer pre-allocation, causes the concurrency
// suite to run extremely slowly. As a workaround we do not pre-allocate in Windows DEBUG builds.
#ifdef _WIN32
    if (kDebugBuild)
        bytesToReserve = 0;
#endif

    // run expects non-const bsonobj
    BSONObj cmd = request.body;

    // run expects const db std::string (can't bind to temporary)
    //获取请求中的DB库信息
    const std::string db = request.getDatabase().toString();

    BSONObjBuilder inPlaceReplyBob = replyBuilder->getInPlaceReplyBuilder(bytesToReserve);

	//ReadConcern检查
    Status rcStatus = waitForReadConcern(
        opCtx, repl::ReadConcernArgs::get(opCtx), command->allowsAfterClusterTime(cmd));
    if (!rcStatus.isOK()) {
        if (rcStatus == ErrorCodes::ExceededTimeLimit) {
            const int debugLevel =
                serverGlobalParams.clusterRole == ClusterRole::ConfigServer ? 0 : 2;
            LOG(debugLevel) << "Command on database " << db
                            << " timed out waiting for read concern to be satisfied. Command: "
                            << redact(command->getRedactedCopyForLogging(request.body));
        }

        auto result = Command::appendCommandStatus(inPlaceReplyBob, rcStatus);
        inPlaceReplyBob.doneFast();
        BSONObjBuilder metadataBob;
        appendReplyMetadataOnError(opCtx, &metadataBob);
        replyBuilder->setMetadata(metadataBob.done());
        return result;
    }

    bool result;
	//该命令是否支持WriteConcern
    if (!command->supportsWriteConcern(cmd)) { //不支持WriteConcern
    	//命令不支持WriteConcern，但是对应的请求中却带有WriteConcern配置，直接报错不支持
        if (commandSpecifiesWriteConcern(cmd)) {
            auto result = Command::appendCommandStatus(
                inPlaceReplyBob,
                {ErrorCodes::InvalidOptions, "Command does not support writeConcern"});
            inPlaceReplyBob.doneFast();
            BSONObjBuilder metadataBob;
            appendReplyMetadataOnError(opCtx, &metadataBob);
            replyBuilder->setMetadata(metadataBob.done());
            return result;
        }

		//Command::publicRun 执行不同命令的run
        result = command->publicRun(opCtx, request, inPlaceReplyBob);
    } else { //支持WriteConcern  
    	//提取WriteConcernOptions信息
        auto wcResult = extractWriteConcern(opCtx, cmd, db);
		//提取异常，直接异常处理
        if (!wcResult.isOK()) {
            auto result = Command::appendCommandStatus(inPlaceReplyBob, wcResult.getStatus());
            inPlaceReplyBob.doneFast();
            BSONObjBuilder metadataBob;
            appendReplyMetadataOnError(opCtx, &metadataBob);
            replyBuilder->setMetadata(metadataBob.done());
            return result;
        }

        auto lastOpBeforeRun = repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp();

        // Change the write concern while running the command.
        const auto oldWC = opCtx->getWriteConcern();
        ON_BLOCK_EXIT([&] { opCtx->setWriteConcern(oldWC); });
        opCtx->setWriteConcern(wcResult.getValue());
        ON_BLOCK_EXIT([&] {
            _waitForWriteConcernAndAddToCommandResponse(
                opCtx, command->getName(), lastOpBeforeRun, &inPlaceReplyBob);
        });

		//执行对应的Command::publicRun
        result = command->publicRun(opCtx, request, inPlaceReplyBob);

        // Nothing in run() should change the writeConcern.
        dassert(SimpleBSONObjComparator::kInstance.evaluate(opCtx->getWriteConcern().toBSON() ==
                                                            wcResult.getValue().toBSON()));
    }

    // When a linearizable read command is passed in, check to make sure we're reading
    // from the primary.
    if (command->supportsNonLocalReadConcern(db, cmd) &&
        (repl::ReadConcernArgs::get(opCtx).getLevel() ==
         repl::ReadConcernLevel::kLinearizableReadConcern) &&
        (request.getCommandName() != "getMore")) {

        auto linearizableReadStatus = waitForLinearizableReadConcern(opCtx);

        if (!linearizableReadStatus.isOK()) {
            inPlaceReplyBob.resetToEmpty();
            auto result = Command::appendCommandStatus(inPlaceReplyBob, linearizableReadStatus);
            inPlaceReplyBob.doneFast();
            BSONObjBuilder metadataBob;
            appendReplyMetadataOnError(opCtx, &metadataBob);
            replyBuilder->setMetadata(metadataBob.done());
            return result;
        }
    }

    Command::appendCommandStatus(inPlaceReplyBob, result);

    auto operationTime = computeOperationTime(
        opCtx, startOperationTime, repl::ReadConcernArgs::get(opCtx).getLevel());

    // An uninitialized operation time means the cluster time is not propagated, so the operation
    // time should not be attached to the response.
    if (operationTime != LogicalTime::kUninitialized &&
        (serverGlobalParams.featureCompatibility.getVersion() ==
         ServerGlobalParams::FeatureCompatibility::Version::kFullyUpgradedTo36)) {
        operationTime.appendAsOperationTime(&inPlaceReplyBob);
    }

    inPlaceReplyBob.doneFast();

    BSONObjBuilder metadataBob;
    appendReplyMetadata(opCtx, request, &metadataBob);
    replyBuilder->setMetadata(metadataBob.done());

    return result;
}

// When active, we won't check if we are master in command dispatch. Activate this if you want to
// test failing during command execution.
MONGO_FP_DECLARE(skipCheckingForNotMasterInCommandDispatch);

/**
 * Executes a command after stripping metadata, performing authorization checks,
 * handling audit impersonation, and (potentially) setting maintenance mode. This method
 * also checks that the command is permissible to run on the node given its current
 * replication state. All the logic here is independent of any particular command; any
 * functionality relevant to a specific command should be confined to its run() method.
 */ 
//mongos流程ServiceEntryPointMongos::handleRequest->Strategy::clientCommand->runCommand
//mongod流程:ServiceEntryPointMongod::handleRequest->runCommands->execCommandDatabase调用
void execCommandDatabase(OperationContext* opCtx,
                         Command* command,
                         const OpMsgRequest& request,
                         rpc::ReplyBuilderInterface* replyBuilder) {

	//初始optime
    auto startOperationTime = getClientOperationTime(opCtx);
    try {
        {
            stdx::lock_guard<Client> lk(*opCtx->getClient());
            CurOp::get(opCtx)->setCommand_inlock(command);
        }

        // TODO: move this back to runCommands when mongos supports OperationContext
        // see SERVER-18515 for details.
        rpc::readRequestMetadata(opCtx, request.body);
        rpc::TrackingMetadata::get(opCtx).initWithOperName(command->getName());

        auto const replCoord = repl::ReplicationCoordinator::get(opCtx);
        initializeOperationSessionInfo(
            opCtx,
            request.body,
            command->requiresAuth(),
            replCoord->getReplicationMode() == repl::ReplicationCoordinator::modeReplSet,
            opCtx->getServiceContext()->getGlobalStorageEngine()->supportsDocLocking());

		//获取dbname
        const auto dbname = request.getDatabase().toString();
		//dbname命名检查
        uassert(
            ErrorCodes::InvalidNamespace,
            str::stream() << "Invalid database name: '" << dbname << "'",
            NamespaceString::validDBName(dbname, NamespaceString::DollarInDbNameBehavior::Allow));

        std::unique_ptr<MaintenanceModeSetter> mmSetter;

        BSONElement cmdOptionMaxTimeMSField;
        BSONElement allowImplicitCollectionCreationField;
        BSONElement helpField;
        BSONElement shardVersionFieldIdx;
        BSONElement queryOptionMaxTimeMSField;

        StringMap<int> topLevelFields;
		//body elem解析
        for (auto&& element : request.body) {
            StringData fieldName = element.fieldNameStringData();
            if (fieldName == QueryRequest::cmdOptionMaxTimeMS) {
                cmdOptionMaxTimeMSField = element;
            } else if (fieldName == "allowImplicitCollectionCreation") {
                allowImplicitCollectionCreationField = element;
            } else if (fieldName == Command::kHelpFieldName) {
                helpField = element;
            } else if (fieldName == ChunkVersion::kShardVersionField) {
                shardVersionFieldIdx = element;
            } else if (fieldName == QueryRequest::queryOptionMaxTimeMS) {
                queryOptionMaxTimeMSField = element;
            }

			//eleme解析异常
            uassert(ErrorCodes::FailedToParse,
                    str::stream() << "Parsed command object contains duplicate top level key: "
                                  << fieldName,
                    topLevelFields[fieldName]++ == 0);
        }

		//如果是help command，则给出相应help应答
        if (Command::isHelpRequest(helpField)) {
            CurOp::get(opCtx)->ensureStarted();
            // We disable last-error for help requests due to SERVER-11492, because config servers
            // use help requests to determine which commands are database writes, and so must be
            // forwarded to all config servers.
            LastError::get(opCtx->getClient()).disable();
            Command::generateHelpResponse(opCtx, replyBuilder, *command);
            return;
        }

        // Session ids are forwarded in requests, so commands that require roundtrips between
        // servers may result in a deadlock when a server tries to check out a session it is already
        // using to service an earlier operation in the command's chain. To avoid this, only check
        // out sessions for commands that require them (i.e. write commands).
        //构造OperationContextSession
        OperationContextSession sessionTxnState(
            opCtx, cmdWhitelist.find(command->getName()) != cmdWhitelist.cend());

        ImpersonationSessionGuard guard(opCtx);
		//权限认证检查
        uassertStatusOK(Command::checkAuthorization(command, opCtx, request));

        const bool iAmPrimary = replCoord->canAcceptWritesForDatabase_UNSAFE(opCtx, dbname);

		//客户端直接链接mongod实例，进行节点状态检查
        if (!opCtx->getClient()->isInDirectClient() &&
            !MONGO_FAIL_POINT(skipCheckingForNotMasterInCommandDispatch)) {

			//是否代码rs.slaveOK设置，如果设置了则即使没有主节点也可以读
            bool commandCanRunOnSecondary = command->slaveOk();

            bool commandIsOverriddenToRunOnSecondary =
                command->slaveOverrideOk() && ReadPreferenceSetting::get(opCtx).canRunOnSecondary();

            bool iAmStandalone = !opCtx->writesAreReplicated();
            bool canRunHere = iAmPrimary || commandCanRunOnSecondary ||
                commandIsOverriddenToRunOnSecondary || iAmStandalone;

            // This logic is clearer if we don't have to invert it.
            if (!canRunHere && command->slaveOverrideOk()) {
                uasserted(ErrorCodes::NotMasterNoSlaveOk, "not master and slaveOk=false");
            }

            if (MONGO_FAIL_POINT(respondWithNotPrimaryInCommandDispatch)) {
                uassert(ErrorCodes::NotMaster, "not primary", canRunHere);
            } else {
                uassert(ErrorCodes::NotMaster, "not master", canRunHere);
            }

            if (!command->maintenanceOk() &&
                replCoord->getReplicationMode() == repl::ReplicationCoordinator::modeReplSet &&
                !replCoord->canAcceptWritesForDatabase_UNSAFE(opCtx, dbname) &&
                !replCoord->getMemberState().secondary()) {

                uassert(ErrorCodes::NotMasterOrSecondary,
                        "node is recovering",
                        !replCoord->getMemberState().recovering());
                uassert(ErrorCodes::NotMasterOrSecondary,
                        "node is not in primary or recovering state",
                        replCoord->getMemberState().primary());
                // Check ticket SERVER-21432, slaveOk commands are allowed in drain mode
                uassert(ErrorCodes::NotMasterOrSecondary,
                        "node is in drain mode",
                        commandIsOverriddenToRunOnSecondary || commandCanRunOnSecondary);
            }
        }

		//只能admin库操作 
        if (command->adminOnly()) {
            LOG(2) << "command: " << request.getCommandName();
        }

		//暂时没用
        if (command->maintenanceMode()) {
            mmSetter.reset(new MaintenanceModeSetter(opCtx));
        }

		//command以下命令不会统计- mongod:Insert  Update  Delete find getmore;  mongos:find  getmore
		//是否进行command统计 CmdInsert  CmdUpdate  CmdDelete  find  getmore不会统计到command统计中，其他命令都会统计到command中
        if (command->shouldAffectCommandCounter()) {
            OpCounters* opCounters = &globalOpCounters;
            opCounters->gotCommand();
			LOG(2) << "yang test .......... command counters:" << request.getCommandName();
        }

        // Handle command option maxTimeMS.
        int maxTimeMS = uassertStatusOK(QueryRequest::parseMaxTimeMS(cmdOptionMaxTimeMSField));

        uassert(ErrorCodes::InvalidOptions,
                "no such command option $maxTimeMs; use maxTimeMS instead",
                queryOptionMaxTimeMSField.eoo());

        if (maxTimeMS > 0) {
            uassert(40119,
                    "Illegal attempt to set operation deadline within DBDirectClient",
                    !opCtx->getClient()->isInDirectClient());
            opCtx->setDeadlineAfterNowBy(Milliseconds{maxTimeMS});
        }

        auto& readConcernArgs = repl::ReadConcernArgs::get(opCtx);
        readConcernArgs = uassertStatusOK(_extractReadConcern(
            request.body, command->supportsNonLocalReadConcern(dbname, request.body)));

        auto& oss = OperationShardingState::get(opCtx);

        if (!opCtx->getClient()->isInDirectClient() &&
            readConcernArgs.getLevel() != repl::ReadConcernLevel::kAvailableReadConcern &&
            (iAmPrimary ||
             ((serverGlobalParams.featureCompatibility.getVersion() ==
               ServerGlobalParams::FeatureCompatibility::Version::kFullyUpgradedTo36) &&
              (readConcernArgs.hasLevel() || readConcernArgs.getArgsClusterTime())))) {
            oss.initializeShardVersion(NamespaceString(command->parseNs(dbname, request.body)),
                                       shardVersionFieldIdx);

            auto const shardingState = ShardingState::get(opCtx);
            if (oss.hasShardVersion()) {
                uassertStatusOK(shardingState->canAcceptShardedCommands());
            }

            // Handle config optime information that may have been sent along with the command.
            uassertStatusOK(shardingState->updateConfigServerOpTimeFromMetadata(opCtx));
        }

        oss.setAllowImplicitCollectionCreation(allowImplicitCollectionCreationField);

        // Can throw
        opCtx->checkForInterrupt();  // May trigger maxTimeAlwaysTimeOut fail point.

        bool retval = false;

        CurOp::get(opCtx)->ensureStarted(); //记录开始时间，结束时间见ServiceEntryPointMongod::handleRequest->currentOp.done()
		//该命令执行次数统计  db.serverStatus().metrics.commands可以获取统计信息
        command->incrementCommandsExecuted();

        if (logger::globalLogDomain()->shouldLog(logger::LogComponent::kTracking,
                                                 logger::LogSeverity::Debug(1)) &&
            rpc::TrackingMetadata::get(opCtx).getParentOperId()) {
            MONGO_LOG_COMPONENT(1, logger::LogComponent::kTracking)
                << rpc::TrackingMetadata::get(opCtx).toString();
            rpc::TrackingMetadata::get(opCtx).setIsLogged(true);
        }

		//真正的命令执行在这里面
        retval = runCommandImpl(opCtx, command, request, replyBuilder, startOperationTime);

		//失败次数统计
        if (!retval) {
            command->incrementCommandsFailed();
        }
    } catch (const DBException& e) {
        // If we got a stale config, wait in case the operation is stuck in a critical section
        if (e.code() == ErrorCodes::StaleConfig) {
            auto sce = dynamic_cast<const StaleConfigException*>(&e);
            invariant(sce);  // do not upcasts from DBException created by uassert variants.

            if (!opCtx->getClient()->isInDirectClient()) {
                ShardingState::get(opCtx)
                    ->onStaleShardVersion(
                        opCtx, NamespaceString(sce->getns()), sce->getVersionReceived())
                    .transitional_ignore();
            }
        }

        BSONObjBuilder metadataBob;
        appendReplyMetadata(opCtx, request, &metadataBob);

        // Note: the read concern may not have been successfully or yet placed on the opCtx, so
        // parsing it separately here.
        const std::string db = request.getDatabase().toString();
        auto readConcernArgsStatus = _extractReadConcern(
            request.body, command->supportsNonLocalReadConcern(db, request.body));
        auto operationTime = readConcernArgsStatus.isOK()
            ? computeOperationTime(
                  opCtx, startOperationTime, readConcernArgsStatus.getValue().getLevel())
            : LogicalClock::get(opCtx)->getClusterTime();

        // An uninitialized operation time means the cluster time is not propagated, so the
        // operation time should not be attached to the error response.
        if (operationTime != LogicalTime::kUninitialized &&
            (serverGlobalParams.featureCompatibility.getVersion() ==
             ServerGlobalParams::FeatureCompatibility::Version::kFullyUpgradedTo36)) {
            LOG(1) << "assertion while executing command '" << request.getCommandName() << "' "
                   << "on database '" << request.getDatabase() << "' "
                   << "with arguments '" << command->getRedactedCopyForLogging(request.body)
                   << "' and operationTime '" << operationTime.toString() << "': " << e.toString();

            _generateErrorResponse(opCtx, replyBuilder, e, metadataBob.obj(), operationTime);
        } else {
            LOG(1) << "assertion while executing command '" << request.getCommandName() << "' "
                   << "on database '" << request.getDatabase() << "' "
                   << "with arguments '" << command->getRedactedCopyForLogging(request.body)
                   << "': " << e.toString();

            _generateErrorResponse(opCtx, replyBuilder, e, metadataBob.obj());
        }
    }
}

/**
 * Fills out CurOp / OpDebug with basic command info.
 */ //可以直接通过CurOp::get(opCtx)获取该类
void curOpCommandSetup(OperationContext* opCtx, const OpMsgRequest& request) {
    auto curop = CurOp::get(opCtx); //CurOp::get
    curop->debug().iscommand = true;

    // We construct a legacy $cmd namespace so we can fill in curOp using
    // the existing logic that existed for OP_QUERY commands
    NamespaceString nss(request.getDatabase(), "$cmd");

    stdx::lock_guard<Client> lk(*opCtx->getClient());
    curop->setOpDescription_inlock(request.body);
    curop->markCommand_inlock();
    curop->setNS_inlock(nss.ns());
}
//mongos流程ServiceEntryPointMongos::handleRequest->Strategy::clientCommand->runCommand
//mongod流程:ServiceEntryPointMongod::handleRequest->runCommands->execCommandDatabase调用

//mongodb语句解析  ServiceEntryPointMongod::handleRequest中执行
DbResponse runCommands(OperationContext* opCtx, const Message& message) {
	//获取message对应的ReplyBuilder，3.6默认对应OpMsgReplyBuilder
    auto replyBuilder = rpc::makeReplyBuilder(rpc::protocolForMessage(message));
    [&] {
        OpMsgRequest request;
        try {  // Parse.
        	//协议解析 根据message获取对应OpMsgRequest
            request = rpc::opMsgRequestFromAnyProtocol(message);
        } catch (const DBException& ex) {
            // If this error needs to fail the connection, propagate it out.
            if (ErrorCodes::isConnectionFatalMessageParseError(ex.code()))
                throw;

            auto operationTime = LogicalClock::get(opCtx)->getClusterTime();
            BSONObjBuilder metadataBob;
            appendReplyMetadataOnError(opCtx, &metadataBob);
            // Otherwise, reply with the parse error. This is useful for cases where parsing fails
            // due to user-supplied input, such as the document too deep error. Since we failed
            // during parsing, we can't log anything about the command.
            LOG(1) << "assertion while parsing command: " << ex.toString();
            _generateErrorResponse(opCtx, replyBuilder.get(), ex, metadataBob.obj(), operationTime);

            return;  // From lambda. Don't try executing if parsing failed.
        }

        try {  // Execute.
        	//opCtx初始化
            curOpCommandSetup(opCtx, request);

            Command* c = nullptr;
            // In the absence of a Command object, no redaction is possible. Therefore
            // to avoid displaying potentially sensitive information in the logs,
            // we restrict the log message to the name of the unrecognized command.
            // However, the complete command object will still be echoed to the client.
            //所有的command都在_commands中保存，查找request对应的命令名是否支持
            if (!(c = Command::findCommand(request.getCommandName()))) { //OpMsgRequest::getCommandName
				//没有找到相应的command的后续处理
                Command::unknownCommands.increment();
                std::string msg = str::stream() << "no such command: '" << request.getCommandName()
                                                << "'";
                LOG(2) << msg;
                uasserted(ErrorCodes::CommandNotFound,
                          str::stream() << msg << ", bad cmd: '" << redact(request.body) << "'");
            }

			//打印命令内容
            LOG(2) << "run command " << request.getDatabase() << ".$cmd" << ' '
                   << c->getRedactedCopyForLogging(request.body) << ' ' << 
                   request.getCommandName(); //如 find  insert等

            {
                // Try to set this as early as possible, as soon as we have figured out the command.
                stdx::lock_guard<Client> lk(*opCtx->getClient());
                CurOp::get(opCtx)->setLogicalOp_inlock(c->getLogicalOp());
            }

            execCommandDatabase(opCtx, c, request, replyBuilder.get());
        } catch (const DBException& ex) {
            BSONObjBuilder metadataBob;
            appendReplyMetadataOnError(opCtx, &metadataBob);
            auto operationTime = LogicalClock::get(opCtx)->getClusterTime();
            LOG(1) << "assertion while executing command '" << request.getCommandName() << "' "
                   << "on database '" << request.getDatabase() << "': " << ex.toString();

            _generateErrorResponse(opCtx, replyBuilder.get(), ex, metadataBob.obj(), operationTime);
        }
    }();

    if (OpMsg::isFlagSet(message, OpMsg::kMoreToCome)) {
        // Close the connection to get client to go through server selection again.
        uassert(ErrorCodes::NotMaster,
                "Not-master error during fire-and-forget command processing",
                !LastError::get(opCtx->getClient()).hadNotMasterError());

        return {};  // Don't reply.
    }

	//OpMsgReplyBuilder::done对数据进行序列化操作
    auto response = replyBuilder->done();
    CurOp::get(opCtx)->debug().responseLength = response.header().dataLen();

    // TODO exhaust
    return DbResponse{std::move(response)};
}

//下面的ServiceEntryPointMongod::handleRequest调用执行 
//解析了接收到的数据,然后调用runQuery负责处理查询
DbResponse receivedQuery(OperationContext* opCtx,
                         const NamespaceString& nss,
                         Client& c,
                         const Message& m) {
    invariant(!nss.isCommand());//这里表明这是一个命令
    globalOpCounters.gotQuery();

    DbMessage d(m);
    QueryMessage q(d);

    CurOp& op = *CurOp::get(opCtx);
    DbResponse dbResponse;

	LOG(1) << "yang test ... receivedQuery";
    try {
        Client* client = opCtx->getClient();
        Status status = AuthorizationSession::get(client)->checkAuthForFind(nss, false);
        audit::logQueryAuthzCheck(client, nss, q.query, status.code());
        uassertStatusOK(status);

        dbResponse.exhaustNS = runQuery(opCtx, q, nss, dbResponse.response);
    } catch (const AssertionException& e) {
        // If we got a stale config, wait in case the operation is stuck in a critical section
        if (!opCtx->getClient()->isInDirectClient() && e.code() == ErrorCodes::StaleConfig) {
            auto& sce = static_cast<const StaleConfigException&>(e);
            ShardingState::get(opCtx)
                ->onStaleShardVersion(opCtx, NamespaceString(sce.getns()), sce.getVersionReceived())
                .transitional_ignore();
        }

        dbResponse.response.reset();
        generateLegacyQueryErrorResponse(&e, q, &op, &dbResponse.response);
    }

    op.debug().responseLength = dbResponse.response.header().dataLen();
    return dbResponse;
}

void receivedKillCursors(OperationContext* opCtx, const Message& m) {
    LastError::get(opCtx->getClient()).disable();
    DbMessage dbmessage(m);
    int n = dbmessage.pullInt();

    uassert(13659, "sent 0 cursors to kill", n != 0);
    massert(13658,
            str::stream() << "bad kill cursors size: " << m.dataSize(),
            m.dataSize() == 8 + (8 * n));
    uassert(13004, str::stream() << "sent negative cursors to kill: " << n, n >= 1);

    if (n > 2000) {
        (n < 30000 ? warning() : error()) << "receivedKillCursors, n=" << n;
        verify(n < 30000);
    }

    const char* cursorArray = dbmessage.getArray(n);

    int found = CursorManager::eraseCursorGlobalIfAuthorized(opCtx, n, cursorArray);

    if (shouldLog(logger::LogSeverity::Debug(1)) || found != n) {
        LOG(found == n ? 1 : 0) << "killcursors: found " << found << " of " << n;
    }
}

//插入 ServiceEntryPointMongod::handleRequest中调用
void receivedInsert(OperationContext* opCtx, const NamespaceString& nsString, const Message& m) {
	//获取m对应的write_ops::Insert类   
	auto insertOp = InsertOp::parseLegacy(m); //通用报文头和报文体解析
	//insert::getNamespace
    invariant(insertOp.getNamespace() == nsString);

    for (const auto& obj : insertOp.getDocuments()) {//insert::getDocuments 获取文档，查看该连接上对应的集合是否已经认证成功
        Status status =
            AuthorizationSession::get(opCtx->getClient())->checkAuthForInsert(opCtx, nsString, obj);
        audit::logInsertAuthzCheck(opCtx->getClient(), nsString, obj, status.code());
        uassertStatusOK(status);
    }
    performInserts(opCtx, insertOp);
}

//数据更新 ServiceEntryPointMongod::handleRequest中调用
void receivedUpdate(OperationContext* opCtx, const NamespaceString& nsString, const Message& m) {
    auto updateOp = UpdateOp::parseLegacy(m); //获取m对应的write_ops::Update类   
    auto& singleUpdate = updateOp.getUpdates()[0];
    invariant(updateOp.getNamespace() == nsString);

    Status status = AuthorizationSession::get(opCtx->getClient())
                        ->checkAuthForUpdate(opCtx,
                                             nsString,
                                             singleUpdate.getQ(),
                                             singleUpdate.getU(),
                                             singleUpdate.getUpsert());
    audit::logUpdateAuthzCheck(opCtx->getClient(),
                               nsString,
                               singleUpdate.getQ(),
                               singleUpdate.getU(),
                               singleUpdate.getUpsert(),
                               singleUpdate.getMulti(),
                               status.code());
    uassertStatusOK(status);

    performUpdates(opCtx, updateOp);
}

//删除 ServiceEntryPointMongod::handleRequest中调用
void receivedDelete(OperationContext* opCtx, const NamespaceString& nsString, const Message& m) {
    auto deleteOp = DeleteOp::parseLegacy(m); //获取该m对应的write_ops::Delete
    auto& singleDelete = deleteOp.getDeletes()[0];
    invariant(deleteOp.getNamespace() == nsString);

    Status status = AuthorizationSession::get(opCtx->getClient())
                        ->checkAuthForDelete(opCtx, nsString, singleDelete.getQ());
    audit::logDeleteAuthzCheck(opCtx->getClient(), nsString, singleDelete.getQ(), status.code());
    uassertStatusOK(status);

    performDeletes(opCtx, deleteOp);
}

//已经查询了数据,这里只是执行得到更多数据的入口   ServiceEntryPointMongod::handleRequest中调用执行
DbResponse receivedGetMore(OperationContext* opCtx,
                           const Message& m,
                           CurOp& curop,
                           bool* shouldLogOpDebug) {
    globalOpCounters.gotGetMore();
    DbMessage d(m);

    const char* ns = d.getns();
    int ntoreturn = d.pullInt();
    uassert(
        34419, str::stream() << "Invalid ntoreturn for OP_GET_MORE: " << ntoreturn, ntoreturn >= 0);
    long long cursorid = d.pullInt64();

    curop.debug().ntoreturn = ntoreturn;
    curop.debug().cursorid = cursorid;

    {
        stdx::lock_guard<Client> lk(*opCtx->getClient());
        CurOp::get(opCtx)->setNS_inlock(ns);
    }

    bool exhaust = false;
    bool isCursorAuthorized = false;

    DbResponse dbresponse;
    try {
        const NamespaceString nsString(ns);
        uassert(ErrorCodes::InvalidNamespace,
                str::stream() << "Invalid ns [" << ns << "]",
                nsString.isValid());

        Status status = AuthorizationSession::get(opCtx->getClient())
                            ->checkAuthForGetMore(nsString, cursorid, false);
        audit::logGetMoreAuthzCheck(opCtx->getClient(), nsString, cursorid, status.code());
        uassertStatusOK(status);

        while (MONGO_FAIL_POINT(rsStopGetMore)) {
            sleepmillis(0);
        }

        dbresponse.response =
            getMore(opCtx, ns, ntoreturn, cursorid, &exhaust, &isCursorAuthorized);
    } catch (AssertionException& e) {
        if (isCursorAuthorized) {
            // If a cursor with id 'cursorid' was authorized, it may have been advanced
            // before an exception terminated processGetMore.  Erase the ClientCursor
            // because it may now be out of sync with the client's iteration state.
            // SERVER-7952
            // TODO Temporary code, see SERVER-4563 for a cleanup overview.
            CursorManager::eraseCursorGlobal(opCtx, cursorid);
        }

        BSONObjBuilder err;
        err.append("$err", e.reason());
        err.append("code", e.code());
        BSONObj errObj = err.obj();

        curop.debug().exceptionInfo = e.toStatus();

        dbresponse = replyToQuery(errObj, ResultFlag_ErrSet);
        curop.debug().responseLength = dbresponse.response.header().dataLen();
        curop.debug().nreturned = 1;
        *shouldLogOpDebug = true;
        return dbresponse;
    }

    curop.debug().responseLength = dbresponse.response.header().dataLen();
    auto queryResult = QueryResult::ConstView(dbresponse.response.buf());
    curop.debug().nreturned = queryResult.getNReturned();

    if (exhaust) {
        curop.debug().exhaust = true;
        dbresponse.exhaustNS = ns;
    }

    return dbresponse;
}

}  // namespace

/* 插入数据过程调用栈
(gdb) bt
#0  mongo::WiredTigerRecordStore::_insertRecords (this=0x7f863ccbdb00, opCtx=opCtx@entry=0x7f8640572640, records=0x7f8640bbd260, timestamps=0x7f863ccda1c0, nRecords=1) at src/mongo/db/storage/wiredtiger/wiredtiger_record_store.cpp:1121
#1  0x00007f8639821154 in mongo::WiredTigerRecordStore::insertRecords (this=<optimized out>, opCtx=opCtx@entry=0x7f8640572640, records=records@entry=0x7f8638e3eae0, timestamps=timestamps@entry=0x7f8638e3eb00, 
    enforceQuota=<optimized out>) at src/mongo/db/storage/wiredtiger/wiredtiger_record_store.cpp:1100
#2  0x00007f8639b30a97 in mongo::CollectionImpl::_insertDocuments (this=this@entry=0x7f863cac7a40, opCtx=opCtx@entry=0x7f8640572640, begin=..., begin@entry=..., end=end@entry=..., enforceQuota=enforceQuota@entry=true, 
    opDebug=0x7f8640645138) at src/mongo/db/catalog/collection_impl.cpp:525
#3  0x00007f8639b311cc in mongo::CollectionImpl::insertDocuments (this=0x7f863cac7a40, opCtx=0x7f8640572640, begin=..., end=..., opDebug=0x7f8640645138, enforceQuota=true, fromMigrate=false)
    at src/mongo/db/catalog/collection_impl.cpp:377
#4  0x00007f8639ac44d2 in insertDocuments (fromMigrate=false, enforceQuota=true, opDebug=<optimized out>, end=..., begin=..., opCtx=0x7f8640572640, this=<optimized out>) at src/mongo/db/catalog/collection.h:498
#5  mongo::(anonymous namespace)::insertDocuments (opCtx=0x7f8640572640, collection=<optimized out>, begin=begin@entry=..., end=end@entry=...) at src/mongo/db/ops/write_ops_exec.cpp:329
#6  0x00007f8639aca1a6 in operator() (__closure=<optimized out>) at src/mongo/db/ops/write_ops_exec.cpp:406
#7  writeConflictRetry<mongo::(anonymous namespace)::insertBatchAndHandleErrors(mongo::OperationContext*, const mongo::write_ops::Insert&, std::vector<mongo::InsertStatement>&, mongo::(anonymous namespace)::LastOpFixer*, mongo::WriteResult*)::<lambda()> > (f=<optimized out>, ns=..., opStr=..., opCtx=0x7f8640572640) at src/mongo/db/concurrency/write_conflict_exception.h:91
#8  insertBatchAndHandleErrors (out=0x7f8638e3ef20, lastOpFixer=0x7f8638e3ef00, batch=..., wholeOp=..., opCtx=0x7f8640572640) at src/mongo/db/ops/write_ops_exec.cpp:418
#9  mongo::performInserts (opCtx=opCtx@entry=0x7f8640572640, wholeOp=...) at src/mongo/db/ops/write_ops_exec.cpp:527
#10 0x00007f8639ab064e in mongo::(anonymous namespace)::CmdInsert::runImpl (this=<optimized out>, opCtx=0x7f8640572640, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:255
#11 0x00007f8639aaa1e8 in mongo::(anonymous namespace)::WriteCommand::enhancedRun (this=<optimized out>, opCtx=0x7f8640572640, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:221
#12 0x00007f863aa7272f in mongo::Command::publicRun (this=0x7f863bd223a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f8640572640, request=..., result=...) at src/mongo/db/commands.cpp:355
#13 0x00007f86399ee834 in runCommandImpl (startOperationTime=..., replyBuilder=0x7f864056f950, request=..., command=0x7f863bd223a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f8640572640)
    at src/mongo/db/service_entry_point_mongod.cpp:506
#14 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7f8640572640, command=command@entry=0x7f863bd223a0 <mongo::(anonymous namespace)::cmdInsert>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:759
#15 0x00007f86399ef39f in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7f8638e3f400) at src/mongo/db/service_entry_point_mongod.cpp:880
#16 0x00007f86399ef39f in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#17 0x00007f86399f0201 in runCommands (message=..., opCtx=0x7f8640572640) at src/mongo/db/service_entry_point_mongod.cpp:890
#18 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7f8640572640, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1163
#19 0x00007f86399fcb3a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f864050c510, guard=...) at src/mongo/transport/service_state_machine.cpp:414
#20 0x00007f86399f7c7f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f864050c510, guard=...) at src/mongo/transport/service_state_machine.cpp:474
#21 0x00007f86399fb6be in operator() (__closure=0x7f8640bbe060) at src/mongo/transport/service_state_machine.cpp:515
#22 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#23 0x00007f863a937c32 in operator() (this=0x7f8638e41550) at /usr/local/include/c++/5.4.0/functional:2267
#24 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f863ccb2480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:125
#25 0x00007f86399f687d in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f864050c510, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:519
#26 0x00007f86399f9211 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7f864050c510, status=...) at src/mongo/transport/service_state_machine.cpp:318
#27 0x00007f86399f9e0b in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7f864050c510, guard=...) at src/mongo/transport/service_state_machine.cpp:276
#28 0x00007f86399f7d11 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f864050c510, guard=...) at src/mongo/transport/service_state_machine.cpp:471
#29 0x00007f86399fb6be in operator() (__closure=0x7f863ccb9a60) at src/mongo/transport/service_state_machine.cpp:515
#30 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#31 0x00007f863a938195 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#32 operator() (__closure=0x7f864052c1b0) at src/mongo/transport/service_executor_synchronous.cpp:143
#33 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#34 0x00007f863ae87d64 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#35 mongo::(anonymous namespace)::runFunc (ctx=0x7f863ccb9c40) at src/mongo/transport/service_entry_point_utils.cpp:55
#36 0x00007f8637b5ce25 in start_thread () from /lib64/libpthread.so.0
#37 0x00007f863788a34d in clone () from /lib64/libc.so.6
*/
//mongos流程ServiceEntryPointMongos::handleRequest->Strategy::clientCommand->runCommand
//mongod流程:ServiceEntryPointMongod::handleRequest->runCommands->execCommandDatabase调用


//ServiceEntryPointMongod::handleRequest(mongod)  ServiceEntryPointMongos::handleRequest(mongos)请求处理

//mongod服务对于客户端请求的处理  ServiceStateMachine::_processMessage或者loopbackBuildResponse中调用
DbResponse ServiceEntryPointMongod::handleRequest(OperationContext* opCtx, const Message& m) {
    // before we lock...
    NetworkOp op = m.operation(); //获取协议头部中的MSGHEADER::Layout.opCode
    bool isCommand = false;
	

	
	//str::stream s;
	//s << "yang test ................ServiceEntryPointMongod::handleRequest op:" << op;
	log() << "yang test ................ServiceEntryPointMongod::handleRequest op:" << (int)op;
	//log() << "yang test ........ServiceEntryPointMongod::handleRequest 11";
	//根据message构造DbMessage
    DbMessage dbmsg(m);
	//根据操作上下文获取对应的client
    Client& c = *opCtx->getClient();  
    //客户端是否直接链接mongod实例,mongos如果做为mongod的客户端则不需要认证
    if (c.isInDirectClient()) {
        invariant(!opCtx->lockState()->inAWriteUnitOfWork());
    } else {
        LastError::get(c).startRequest();
		//AuthorizationSession::startRequest
        AuthorizationSession::get(c)->startRequest(opCtx);

        // We should not be holding any locks at this point
        invariant(!opCtx->lockState()->isLocked());
    }

	//获取库.表信息，注意只有dbUpdate<opCode<dbDelete的opCode请求才通过dbmsg直接获取库和表信息
    const char* ns = dbmsg.messageShouldHaveNs() ? dbmsg.getns() : NULL;
    const NamespaceString nsString = ns ? NamespaceString(ns) : NamespaceString();

    if (op == dbQuery) {
		//例如admin.$cmd
        if (nsString.isCommand()) {
            isCommand = true;
        }
    } else if (op == dbCommand || op == dbMsg) {
        //3.6走这里
        isCommand = true;
    }

	//获取对应得currentOp
    CurOp& currentOp = *CurOp::get(opCtx);
    {
        stdx::lock_guard<Client> lk(*opCtx->getClient());
        // Commands handling code will reset this if the operation is a command
        // which is logically a basic CRUD operation like query, insert, etc.
        currentOp.setNetworkOp_inlock(op);
        currentOp.setLogicalOp_inlock(networkOpToLogicalOp(op));
    }
	
    //CurOp::debug 初始化opDebug，慢日志相关记录
    OpDebug& debug = currentOp.debug();

	//启动的时候设置的参数默认是100ms,当操作超过了这个时间且启动时设置--profile为1或者2  
    long long logThresholdMs = serverGlobalParams.slowMS;
	//时mongodb将记录这次慢操作,1为只记录慢操作,即操作时间大于了设置的配置,2表示记录所有操作  
    bool shouldLogOpDebug = shouldLog(logger::LogSeverity::Debug(1));

    DbResponse dbresponse;
    if (op == dbMsg || op == dbCommand || (op == dbQuery && isCommand)) {
        dbresponse = runCommands(opCtx, m);   //runCommands   新版本插入 查询过程实际上走这里面
    } else if (op == dbQuery) {
        invariant(!isCommand);
		//查询可能走这里，也可能 实际上新版本走的是FindCmd::run(runCommands层层进入)，不在走这里
        dbresponse = receivedQuery(opCtx, nsString, c, m);
    } else if (op == dbGetMore) { //已经查询了数据,这里只是执行得到更多数据的入口  
        dbresponse = receivedGetMore(opCtx, m, currentOp, &shouldLogOpDebug);
    } else {
        // The remaining operations do not return any response. They are fire-and-forget.
        try {
            if (op == dbKillCursors) {
                currentOp.ensureStarted();
                logThresholdMs = 10;
                receivedKillCursors(opCtx, m);
            } else if (op != dbInsert && op != dbUpdate && op != dbDelete) {
                log() << "    operation isn't supported: " << static_cast<int>(op);
                currentOp.done();
                shouldLogOpDebug = true;
            } else {
                if (!opCtx->getClient()->isInDirectClient()) {
                    uassert(18663,
                            str::stream() << "legacy writeOps not longer supported for "
                                          << "versioned connections, ns: "
                                          << nsString.ns()
                                          << ", op: "
                                          << networkOpToString(op),
                            !ShardedConnectionInfo::get(&c, false));
                }

                if (!nsString.isValid()) {
                    uassert(16257, str::stream() << "Invalid ns [" << ns << "]", false);
                } else if (op == dbInsert) {
                    receivedInsert(opCtx, nsString, m); //插入操作入口   新版本CmdInsert::runImpl
                } else if (op == dbUpdate) {
                    receivedUpdate(opCtx, nsString, m); //更新操作入口  
                } else if (op == dbDelete) {
                    receivedDelete(opCtx, nsString, m); //删除操作入口  
                } else {
                    invariant(false);
                }
            }
        } catch (const AssertionException& ue) {
            LastError::get(c).setLastError(ue.code(), ue.reason());
            LOG(3) << " Caught Assertion in " << networkOpToString(op) << ", continuing "
                   << redact(ue);
            debug.exceptionInfo = ue.toStatus();
        }
    }

	//计时处理
    currentOp.ensureStarted();
	//CurOp::done
    currentOp.done(); //结束时间确定，开始时间在//execCommandDatabase->ensureStarted
	//获取runCommands执行时间，也就是内部处理时间
    debug.executionTimeMicros = durationCount<Microseconds>(currentOp.elapsedTimeExcludingPauses());

	//mongod读写的时间延迟统计  db.serverStatus().opLatencies  
    Top::get(opCtx->getServiceContext())
        .incrementGlobalLatencyStats(
            opCtx,
            durationCount<Microseconds>(currentOp.elapsedTimeExcludingPauses()),
            currentOp.getReadWriteType());

    const bool shouldSample = serverGlobalParams.sampleRate == 1.0
        ? true
        : c.getPrng().nextCanonicalDouble() < serverGlobalParams.sampleRate;
 
	//慢日志记录  slowlog record     慢日志除了这里打印外，update和delete操作的打印在finishCurOp
    if (shouldLogOpDebug || (shouldSample && debug.executionTimeMicros > logThresholdMs * 1000LL)) {
        Locker::LockerInfo lockerInfo;  
		//OperationContext::lockState  LockerImpl<>::getLockerInfo
        opCtx->lockState()->getLockerInfo(&lockerInfo); 

		//OpDebug::report
        log() << debug.report(&c, currentOp, lockerInfo.stats); //记录慢日志到日志文件
    }

	//记录慢日志到system.profile集合 
    if (currentOp.shouldDBProfile(shouldSample)) {
        // Performance profiling is on
        if (opCtx->lockState()->isReadLocked()) {
            LOG(1) << "note: not profiling because recursive read lock";
        } else if (lockedForWriting()) {
            // TODO SERVER-26825: Fix race condition where fsyncLock is acquired post
            // lockedForWriting() call but prior to profile collection lock acquisition.
            LOG(1) << "note: not profiling because doing fsync+lock";
        } else if (storageGlobalParams.readOnly) {
            LOG(1) << "note: not profiling because server is read-only";
        } else {
            profile(opCtx, op); //db.system.profile.find().pretty()中查看，记录慢日志到这个集合
        }
    }
	//各种统计信息
    recordCurOpMetrics(opCtx);
    return dbresponse;
}

}  // namespace mongo
