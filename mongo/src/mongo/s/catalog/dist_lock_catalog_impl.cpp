/**
 *    Copyright (C) 2015 MongoDB Inc.
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

#include "mongo/platform/basic.h"

#include "mongo/s/catalog/dist_lock_catalog_impl.h"

#include <string>

#include "mongo/base/status.h"
#include "mongo/base/status_with.h"
#include "mongo/bson/util/bson_extract.h"
#include "mongo/client/read_preference.h"
#include "mongo/db/lasterror.h"
#include "mongo/db/query/find_and_modify_request.h"
#include "mongo/db/repl/read_concern_args.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/rpc/metadata.h"
#include "mongo/rpc/metadata/repl_set_metadata.h"
#include "mongo/rpc/metadata/sharding_metadata.h"
#include "mongo/s/catalog/type_lockpings.h"
#include "mongo/s/catalog/type_locks.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/s/write_ops/batched_command_request.h"
#include "mongo/s/write_ops/batched_command_response.h"
#include "mongo/util/time_support.h"

namespace mongo {

using std::string;
using std::vector;

namespace {

const char kFindAndModifyResponseResultDocField[] = "value";
const char kLocalTimeField[] = "localTime";

const ReadPreferenceSetting kReadPref(ReadPreference::PrimaryOnly, TagSet());

/**
 * Returns the resulting new object from the findAndModify response object.
 * Returns LockStateChangeFailed if value field was null, which indicates that
 * the findAndModify command did not modify any document.
 * This also checks for errors in the response object.
 */
//提取findAndModify结果
StatusWith<BSONObj> extractFindAndModifyNewObj(StatusWith<Shard::CommandResponse> response) {
    if (!response.isOK()) {
        return response.getStatus();
    }
    if (!response.getValue().commandStatus.isOK()) {
        return response.getValue().commandStatus;
    }
    if (!response.getValue().writeConcernStatus.isOK()) {
        return response.getValue().writeConcernStatus;
    }

    auto responseObj = std::move(response.getValue().response);

    if (const auto& newDocElem = responseObj[kFindAndModifyResponseResultDocField]) {
        if (newDocElem.isNull()) {
            return {ErrorCodes::LockStateChangeFailed,
                    "findAndModify query predicate didn't match any lock document"};
        }

        if (!newDocElem.isABSONObj()) {
            return {ErrorCodes::UnsupportedFormat,
                    str::stream() << "expected an object from the findAndModify response '"
                                  << kFindAndModifyResponseResultDocField
                                  << "'field, got: "
                                  << newDocElem};
        }

        return newDocElem.Obj().getOwned();
    }

    return {ErrorCodes::UnsupportedFormat,
            str::stream() << "no '" << kFindAndModifyResponseResultDocField
                          << "' in findAndModify response"};
}

/**
 * Extract the electionId from a serverStatus command response.
 xx:PRIMARY> db.serverStatus().repl
{
        "hosts" : [
                "xxx:20002",
                "xxxx:20001",
                "xxxx:20000"
        ],
        "setName" : "xxx",
        "setVersion" : 4,
        "ismaster" : true,
        "secondary" : false,
        "primary" : "xxxx:20001",
        "me" : "xxx:20001",
        "electionId" : ObjectId("7fffffff0000000000000004"),
        "lastWrite" : {
                "opTime" : {
                        "ts" : Timestamp(1596901072, 267),
                        "t" : NumberLong(4)
                },
                "lastWriteDate" : ISODate("2020-08-08T15:37:52Z"),
                "majorityOpTime" : {
                        "ts" : Timestamp(1596901072, 259),
                        "t" : NumberLong(4)
                },
                "majorityWriteDate" : ISODate("2020-08-08T15:37:52Z")
        },
        "rbid" : 1
}
 */ //解析serverstatus
StatusWith<OID> extractElectionId(const BSONObj& responseObj) {
    BSONElement replElem;
    auto replElemStatus = bsonExtractTypedField(responseObj, "repl", Object, &replElem);

    if (!replElemStatus.isOK()) {
        return {ErrorCodes::UnsupportedFormat, replElemStatus.reason()};
    }

    const auto replSubObj = replElem.Obj();
    OID electionId;
	//db.serverStatus().repl.electionId字段
    auto electionIdStatus = bsonExtractOIDField(replSubObj, "electionId", &electionId);

    if (!electionIdStatus.isOK()) { //解析异常
        // Secondaries don't have electionId.
        if (electionIdStatus.code() == ErrorCodes::NoSuchKey) {
            // Verify that the from replSubObj that this is indeed not a primary.
            bool isPrimary = false;
            auto isPrimaryStatus = bsonExtractBooleanField(replSubObj, "ismaster", &isPrimary);

            if (!isPrimaryStatus.isOK()) {
                return {ErrorCodes::UnsupportedFormat, isPrimaryStatus.reason()};
            }

			//解析到如果是master节点，则继续获取me，也就是本master地址
            if (isPrimary) {
                string hostContacted;
                auto hostContactedStatus = bsonExtractStringField(replSubObj, "me", &hostContacted);

                if (!hostContactedStatus.isOK()) {
                    return {
                        ErrorCodes::UnsupportedFormat,
                        str::stream()
                            << "failed to extract 'me' field from repl subsection of serverStatus: "
                            << hostContactedStatus.reason()};
                }

                return {ErrorCodes::UnsupportedFormat,
                        str::stream() << "expected primary to have electionId but not present on "
                                      << hostContacted};
            }

            return {ErrorCodes::NotMaster, "only primary can have electionId"};
        }

        return {ErrorCodes::UnsupportedFormat, electionIdStatus.reason()};
    }

	//返回electionId
    return electionId;
}

}  // unnamed namespace

//"config.lockpings"   "config.locks";
DistLockCatalogImpl::DistLockCatalogImpl()
    : _lockPingNS(LockpingsType::ConfigNS), _locksNS(LocksType::ConfigNS) {}

DistLockCatalogImpl::~DistLockCatalogImpl() = default;


/*
mongos> db.lockpings.find()
{ "_id" : "ConfigServer", "ping" : ISODate("2020-08-08T15:46:43.471Z") }
{ "_id" : "xxx:20003:1581573543:3630770638362238126", "ping" : ISODate("2020-08-08T15:46:36.839Z") }
{ "_id" : "xxx:20003:1581573552:3015220040157753333", "ping" : ISODate("2020-02-13T14:31:15.194Z") }
{ "_id" : "xxx:20003:1581573564:-3080866622298223419", "ping" : ISODate("2020-08-08T15:46:40.320Z") }
{ "_id" : "xxx:20001:1581573577:-6950517477465643150", "ping" : ISODate("2020-08-08T15:46:33.676Z") }
{ "_id" : "xxx:20001:1581573577:-4720166468454920588", "ping" : ISODate("2020-02-13T08:18:07.712Z") }
{ "_id" : "xxx:20001:1581573577:6146141285149556418", "ping" : ISODate("2020-08-08T15:46:36.501Z") }
{ "_id" : "xxx:20001:1581602007:2653463530376788741", "ping" : ISODate("2020-08-08T15:46:27.902Z") }
{ "_id" : "xxx:20003:1581604307:-5313333738365382099", "ping" : ISODate("2020-08-08T15:46:33.679Z") }
mongos> 

*/
//根据processID从config.pings表查找对应数据  
//ReplSetDistLockManager::isLockExpired调用
StatusWith<LockpingsType> DistLockCatalogImpl::getPing(OperationContext* opCtx,
                                                       StringData processID) {
	//本文件的DistLockCatalogImpl::_findOnConfig
	auto findResult = _findOnConfig(
        opCtx, kReadPref, _lockPingNS, BSON(LockpingsType::process() << processID), BSONObj(), 1);

    if (!findResult.isOK()) {
        return findResult.getStatus();
    }

    const auto& findResultSet = findResult.getValue();

    if (findResultSet.empty()) {
        return {ErrorCodes::NoMatchingDocument,
                str::stream() << "ping entry for " << processID << " not found"};
    }

    BSONObj doc = findResultSet.front();
    auto pingDocResult = LockpingsType::fromBSON(doc);
    if (!pingDocResult.isOK()) {
        return {ErrorCodes::FailedToParse,
                str::stream() << "failed to parse document: " << doc << " : "
                              << pingDocResult.getStatus().toString()};
    }

    return pingDocResult.getValue();
}

/*
const BSONField<std::string> LockpingsType::process("_id");
const BSONField<Date_t> LockpingsType::ping("ping");
*/
//根据processID从config.lockpings中查找_id:processID对应数据的ping字段内容为ping
Status DistLockCatalogImpl::ping(OperationContext* opCtx, StringData processID, Date_t ping) {
    auto request =
        FindAndModifyRequest::makeUpdate(_lockPingNS,
                                         BSON(LockpingsType::process() << processID),
                                         BSON("$set" << BSON(LockpingsType::ping(ping))));
    request.setUpsert(true);
    request.setWriteConcern(kMajorityWriteConcern);

    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto resultStatus = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        _locksNS.db().toString(),
        request.toBSON(),
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kNotIdempotent);

    auto findAndModifyStatus = extractFindAndModifyNewObj(std::move(resultStatus));
    return findAndModifyStatus.getStatus();
}

/*
mongos> use test3
switched to db test3
mongos>  db.test3.insert({ "_id" : "test2-movePrimary", "state" : 0, "process" : "ConfigServer", "ts" : ObjectId("5f2a8d3abd120dda8009f247"), "when" : ISODate("2020-08-05T10:43:06.205Z"), "who" : "ConfigServer:conn731", "why" : "enableSharding" })
WriteResult({ "nInserted" : 1 })
mongos> 
mongos> 
mongos>  db.test3.findAndModify ( {query: { _id: "test2-movePrimary", state: 0 },update: { $set: { ts: ObjectId('5f2a8d3abd120dda8009f247'), state: 2, who: "ConfigServer:conn731", process: "ConfigServer", when: new Date(1596624186205), why: "enableSharding" } }, upsert: true, new: true})
{
        "_id" : "test2-movePrimary",
        "state" : 2,
        "process" : "ConfigServer",
        "ts" : ObjectId("5f2a8d3abd120dda8009f247"),
        "when" : ISODate("2020-08-05T10:43:06.205Z"),
        "who" : "ConfigServer:conn731",
        "why" : "enableSharding"
}
mongos> 
mongos> 
mongos> db.test3.findAndModify ( {query: { _id: "test2-movePrimary", state: 0 },update: { $set: { ts: ObjectId('5f2a8d3abd120dda8009f247'), state: 2, who: "ConfigServer:conn731", process: "ConfigServer", when: new Date(1596624186205), why: "enableSharding" } }, upsert: true, new: true})
2020-08-05T18:58:00.251+0800 E QUERY    [thread1] Error: findAndModifyFailed failed: {
        "ok" : 0,
        "errmsg" : "E11000 duplicate key error collection: test3.test3 index: _id_ dup key: { : \"test2-movePrimary\" }",
        "code" : 11000,
        "codeName" : "DuplicateKey",
        "operationTime" : Timestamp(1596625067, 2),
        "$clusterTime" : {
                "clusterTime" : Timestamp(1596625074, 1),
                "signature" : {
                        "hash" : BinData(0,"rv/1oEHfIuhjI2QFhzv6/Gnx2FQ="),
                        "keyId" : NumberLong("6854353039424225306")
                }
        }
} :
_getErrorWithCode@src/mongo/shell/utils.js:25:13
DBCollection.prototype.findAndModify@src/mongo/shell/collection.js:724:1
@(shell):1:1
mongos> 
*/
//通过findAndModify来获取对应锁，也就是更新Locks表中id:lockID这个文档为新的内容，内容如下newLockDetails
//ReplSetDistLockManager::lockWithSessionID调用
StatusWith<LocksType> DistLockCatalogImpl::grabLock(OperationContext* opCtx,
                                                    StringData lockID,
                                                    const OID& lockSessionID,
                                                    StringData who,
                                                    StringData processId,
                                                    Date_t time,
                                                    StringData why,
                                                    const WriteConcernOptions& writeConcern) {
	//更新Locks表中id:lockID这个文档为新的内容，内容如下newLockDetails
	BSONObj newLockDetails(BSON(
        LocksType::lockID(lockSessionID) << LocksType::state(LocksType::LOCKED) << LocksType::who()
                                         << who
                                         << LocksType::process()
                                         << processId
                                         << LocksType::when(time)
                                         << LocksType::why()
                                         << why));

	/*
	findAndModify { findAndModify: "locks", query: { _id: "test2-movePrimary", state: 0 }, update: 
	{ $set: { ts: ObjectId('5f2a8d3abd120dda8009f247'), state: 2, who: "ConfigServer:conn731", 
	process: "ConfigServer", when: new Date(1596624186205), why: "enableSharding" } }, upsert: true, 
	new: true, writeConcern: { w: "majority", wtimeout: 15000 }, $db: "config" }
	*/
	//构造FindAndModifyRequest
    auto request = FindAndModifyRequest::makeUpdate(
        _locksNS,
        //查询条件{ts:lockSessionID, _id:name}
        BSON(LocksType::name() << lockID << LocksType::state(LocksType::UNLOCKED)),
        //更新字段 stats:newLockDetails
        BSON("$set" << newLockDetails));
    request.setUpsert(true);
    request.setShouldReturnNew(true);
    request.setWriteConcern(writeConcern);

    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
	//执行findAndModify命令并获取对应结果
    auto resultStatus = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        _locksNS.db().toString(),
        request.toBSON(),
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kNoRetry);  // Dist lock manager is handling own retries

	//提取findAndModify这个原子操作的结果
    auto findAndModifyStatus = extractFindAndModifyNewObj(std::move(resultStatus));
    if (!findAndModifyStatus.isOK()) {
		//重复key，说明其他操作获取到了锁
        if (findAndModifyStatus == ErrorCodes::DuplicateKey) {
            // Another thread won the upsert race. Also see SERVER-14322.
            return {ErrorCodes::LockStateChangeFailed,
                    str::stream() << "duplicateKey error during upsert of lock: " << lockID};
        }

        return findAndModifyStatus.getStatus();
    }

    BSONObj doc = findAndModifyStatus.getValue();
	//LocksType::fromBSON解析返回结果
    auto locksTypeResult = LocksType::fromBSON(doc);
    if (!locksTypeResult.isOK()) { 
		//findAndModify返回结果不符合协议格式 
        return {ErrorCodes::FailedToParse,
                str::stream() << "failed to parse: " << doc << " : "
                              << locksTypeResult.getStatus().toString()};
    }

	//返回对应结果   
    return locksTypeResult.getValue();
}

//ReplSetDistLockManager::lockWithSessionID中调用
StatusWith<LocksType> DistLockCatalogImpl::overtakeLock(OperationContext* opCtx,
                                                        StringData lockID,
                                                        const OID& lockSessionID,
                                                        const OID& currentHolderTS,
                                                        StringData who,
                                                        StringData processId,
                                                        Date_t time,
                                                        StringData why) {
    BSONArrayBuilder orQueryBuilder;
    orQueryBuilder.append(
        BSON(LocksType::name() << lockID << LocksType::state(LocksType::UNLOCKED)));
    orQueryBuilder.append(BSON(LocksType::name() << lockID << LocksType::lockID(currentHolderTS)));

    BSONObj newLockDetails(BSON(
        LocksType::lockID(lockSessionID) << LocksType::state(LocksType::LOCKED) << LocksType::who()
                                         << who
                                         << LocksType::process()
                                         << processId
                                         << LocksType::when(time)
                                         << LocksType::why()
                                         << why));

    auto request = FindAndModifyRequest::makeUpdate(
        _locksNS, BSON("$or" << orQueryBuilder.arr()), BSON("$set" << newLockDetails));
    request.setShouldReturnNew(true);
    request.setWriteConcern(kMajorityWriteConcern);

    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto resultStatus = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        _locksNS.db().toString(),
        request.toBSON(),
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kNotIdempotent);

    auto findAndModifyStatus = extractFindAndModifyNewObj(std::move(resultStatus));
    if (!findAndModifyStatus.isOK()) {
        return findAndModifyStatus.getStatus();
    }

    BSONObj doc = findAndModifyStatus.getValue();
    auto locksTypeResult = LocksType::fromBSON(doc);
    if (!locksTypeResult.isOK()) {
        return {ErrorCodes::FailedToParse,
                str::stream() << "failed to parse: " << doc << " : "
                              << locksTypeResult.getStatus().toString()};
    }

    return locksTypeResult.getValue();
}

//config.locks表中的{ts:lockSessionID}这条数据对应的stat字段设置为0，也就是解锁
Status DistLockCatalogImpl::unlock(OperationContext* opCtx, const OID& lockSessionID) {
    FindAndModifyRequest request = FindAndModifyRequest::makeUpdate(
        _locksNS,
        BSON(LocksType::lockID(lockSessionID)),
        BSON("$set" << BSON(LocksType::state(LocksType::UNLOCKED))));
    request.setWriteConcern(kMajorityWriteConcern);
    return _unlock(opCtx, request);
}

//ReplSetDistLockManager::lockWithSessionID中调用
//config.locks表中的{ts:lockSessionID, _id:name}这条数据对应的stat字段设置为0，也就是解锁
Status DistLockCatalogImpl::unlock(OperationContext* opCtx,
                                   const OID& lockSessionID,
                                   StringData name) {
    FindAndModifyRequest request = FindAndModifyRequest::makeUpdate(
        _locksNS,
        //查询条件{ts:lockSessionID, _id:name}
        BSON(LocksType::lockID(lockSessionID) << LocksType::name(name.toString())),
        //更新字段state
        BSON("$set" << BSON(LocksType::state(LocksType::UNLOCKED))));
    request.setWriteConcern(kMajorityWriteConcern);
    return _unlock(opCtx, request);
}

//上面的DistLockCatalogImpl::unlock调用
Status DistLockCatalogImpl::_unlock(OperationContext* opCtx, const FindAndModifyRequest& request) {
    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto resultStatus = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        _locksNS.db().toString(),
        request.toBSON(),
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kIdempotent);

    auto findAndModifyStatus = extractFindAndModifyNewObj(std::move(resultStatus));
    if (findAndModifyStatus == ErrorCodes::LockStateChangeFailed) {
        // Did not modify any document, which implies that the lock already has a
        // a different owner. This is ok since it means that the objective of
        // releasing ownership of the lock has already been accomplished.
        return Status::OK();
    }

    return findAndModifyStatus.getStatus();
}

//把config.locks表的所有数据的state值为0
Status DistLockCatalogImpl::unlockAll(OperationContext* opCtx, const std::string& processID) {
    BatchedCommandRequest request([&] {
        write_ops::Update updateOp(_locksNS);
        updateOp.setUpdates({[&] {
            write_ops::UpdateOpEntry entry;
            entry.setQ(BSON(LocksType::process(processID)));
            entry.setU(BSON("$set" << BSON(LocksType::state(LocksType::UNLOCKED))));
            entry.setUpsert(false);
            entry.setMulti(true);
            return entry;
        }()});
        return updateOp;
    }());
    request.setWriteConcern(kLocalWriteConcern.toBSON());

    BSONObj cmdObj = request.toBSON();

    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto response = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        _locksNS.db().toString(),
        cmdObj,
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kIdempotent);

    if (!response.isOK()) {
        return response.getStatus();
    }
    if (!response.getValue().commandStatus.isOK()) {
        return response.getValue().commandStatus;
    }
    if (!response.getValue().writeConcernStatus.isOK()) {
        return response.getValue().writeConcernStatus;
    }

    BatchedCommandResponse batchResponse;
    std::string errmsg;
    if (!batchResponse.parseBSON(response.getValue().response, &errmsg)) {
        return Status(ErrorCodes::FailedToParse,
                      str::stream()
                          << "Failed to parse config server response to batch request for "
                             "unlocking existing distributed locks"
                          << causedBy(errmsg));
    }
    return batchResponse.toStatus();
}

/*
XXX:PRIMARY> db.serverStatus().repl.electionId
ObjectId("7fffffff0000000000000004")
XXX:PRIMARY> db.serverStatus().localTime
ISODate("2020-08-08T16:11:15.019Z")
XXX:PRIMARY> 
*/
//从serverstatus中解析出localTime和repl.electionId字段
//ReplSetDistLockManager::isLockExpired调用
StatusWith<DistLockCatalog::ServerInfo> DistLockCatalogImpl::getServerInfo(
    OperationContext* opCtx) {
    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto resultStatus = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        kReadPref,
        "admin",
        BSON("serverStatus" << 1),
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kIdempotent);

    if (!resultStatus.isOK()) {
        return resultStatus.getStatus();
    }
    if (!resultStatus.getValue().commandStatus.isOK()) {
        return resultStatus.getValue().commandStatus;
    }

    BSONObj responseObj(std::move(resultStatus.getValue().response));

    BSONElement localTimeElem;
    auto localTimeStatus =
		//const char kLocalTimeField[] = "localTime"; 解析出localTime
        bsonExtractTypedField(responseObj, kLocalTimeField, Date, &localTimeElem);

    if (!localTimeStatus.isOK()) {
        return {ErrorCodes::UnsupportedFormat, localTimeStatus.reason()};
    }

	//解析出db.serverStatus().repl.electionId信息
    auto electionIdStatus = extractElectionId(responseObj);

    if (!electionIdStatus.isOK()) {
        return electionIdStatus.getStatus();
    }

    return DistLockCatalog::ServerInfo(localTimeElem.date(), electionIdStatus.getValue());
}

//根据ts:lockSessionID在config.locks中查找
StatusWith<LocksType> DistLockCatalogImpl::getLockByTS(OperationContext* opCtx,
                                                       const OID& lockSessionID) {
	//本文件的DistLockCatalogImpl::_findOnConfig
    auto findResult = _findOnConfig(
        opCtx, kReadPref, _locksNS, BSON(LocksType::lockID(lockSessionID)), BSONObj(), 1);

    if (!findResult.isOK()) {
        return findResult.getStatus();
    }

    const auto& findResultSet = findResult.getValue();

    if (findResultSet.empty()) {
        return {ErrorCodes::LockNotFound,
                str::stream() << "lock with ts " << lockSessionID << " not found"};
    }

    BSONObj doc = findResultSet.front();
    auto locksTypeResult = LocksType::fromBSON(doc);
    if (!locksTypeResult.isOK()) {
        return {ErrorCodes::FailedToParse,
                str::stream() << "failed to parse: " << doc << " : "
                              << locksTypeResult.getStatus().toString()};
    }

    return locksTypeResult.getValue();
}

//ReplSetDistLockManager::lockWithSessionID调用
//根据name在config.locks表中查找
StatusWith<LocksType> DistLockCatalogImpl::getLockByName(OperationContext* opCtx, StringData name) {
    auto findResult =
		//本文件的DistLockCatalogImpl::_findOnConfig
        _findOnConfig(opCtx, kReadPref, _locksNS, BSON(LocksType::name() << name), BSONObj(), 1);

    if (!findResult.isOK()) {
        return findResult.getStatus();
    }

    const auto& findResultSet = findResult.getValue();

    if (findResultSet.empty()) {
        return {ErrorCodes::LockNotFound,
                str::stream() << "lock with name " << name << " not found"};
    }

    BSONObj doc = findResultSet.front();
    auto locksTypeResult = LocksType::fromBSON(doc);
    if (!locksTypeResult.isOK()) {
        return {ErrorCodes::FailedToParse,
                str::stream() << "failed to parse: " << doc << " : "
                              << locksTypeResult.getStatus().toString()};
    }

    return locksTypeResult.getValue();
}

Status DistLockCatalogImpl::stopPing(OperationContext* opCtx, StringData processId) {
    auto request =
        FindAndModifyRequest::makeRemove(_lockPingNS, BSON(LockpingsType::process() << processId));
    request.setWriteConcern(kMajorityWriteConcern);

    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto resultStatus = shardRegistry->getConfigShard()->runCommandWithFixedRetryAttempts(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        _locksNS.db().toString(),
        request.toBSON(),
        Shard::kDefaultConfigCommandTimeout,
        Shard::RetryPolicy::kNotIdempotent);

    auto findAndModifyStatus = extractFindAndModifyNewObj(std::move(resultStatus));
    return findAndModifyStatus.getStatus();
}

//从nss中根据query查询，然后sort排序，取limit条数据  
//本文件中的DistLockCatalogImpl::getPing  DistLockCatalogImpl::getLockByTS  DistLockCatalogImpl::getLockByName调用
StatusWith<vector<BSONObj>> DistLockCatalogImpl::_findOnConfig(
    OperationContext* opCtx,
    const ReadPreferenceSetting& readPref,
    const NamespaceString& nss,
    const BSONObj& query,
    const BSONObj& sort,
    boost::optional<long long> limit) {
    auto const shardRegistry = Grid::get(opCtx)->shardRegistry();
    auto result = shardRegistry->getConfigShard()->exhaustiveFindOnConfig(
        opCtx, readPref, repl::ReadConcernLevel::kMajorityReadConcern, nss, query, sort, limit);
    if (!result.isOK()) {
        return result.getStatus();
    }

    return result.getValue().docs;
}

}  // namespace mongo
