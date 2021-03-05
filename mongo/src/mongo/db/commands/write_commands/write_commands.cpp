/**
 *    Copyright (C) 2013 10gen Inc.
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

#include "mongo/base/init.h"
#include "mongo/bson/mutable/document.h"
#include "mongo/bson/mutable/element.h"
#include "mongo/db/catalog/database_holder.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/commands/write_commands/write_commands_common.h"
#include "mongo/db/curop.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/json.h"
#include "mongo/db/lasterror.h"
#include "mongo/db/ops/delete_request.h"
#include "mongo/db/ops/parsed_delete.h"
#include "mongo/db/ops/parsed_update.h"
#include "mongo/db/ops/update_lifecycle_impl.h"
#include "mongo/db/ops/write_ops.h"
#include "mongo/db/ops/write_ops_exec.h"
#include "mongo/db/query/explain.h"
#include "mongo/db/query/get_executor.h"
#include "mongo/db/repl/repl_client_info.h"
#include "mongo/db/repl/replication_coordinator_global.h"
#include "mongo/db/server_parameters.h"
#include "mongo/db/stats/counters.h"
#include "mongo/db/write_concern.h"
#include "mongo/s/stale_exception.h"

namespace mongo {
namespace {

void redactTooLongLog(mutablebson::Document* cmdObj, StringData fieldName) {
    namespace mmb = mutablebson;
    mmb::Element root = cmdObj->root();
    mmb::Element field = root.findFirstChildNamed(fieldName);

    // If the cmdObj is too large, it will be a "too big" message given by CachedBSONObj.get()
    if (!field.ok()) {
        return;
    }

    // Redact the log if there are more than one documents or operations.
    if (field.countChildren() > 1) {
        field.setValueInt(field.countChildren()).transitional_ignore();
    }
}

Status checkAuthForWriteCommand(Client* client,
                                BatchedCommandRequest::BatchType batchType,
                                const OpMsgRequest& request) {
    Status status =
        auth::checkAuthForWriteCommand(AuthorizationSession::get(client), batchType, request);
    if (!status.isOK()) {
        LastError::get(client).setLastError(status.code(), status.reason());
    }
    return status;
}

bool shouldSkipOutput(OperationContext* opCtx) {
    const WriteConcernOptions& writeConcern = opCtx->getWriteConcern();
    return writeConcern.wMode.empty() && writeConcern.wNumNodes == 0 &&
        (writeConcern.syncMode == WriteConcernOptions::SyncMode::NONE ||
         writeConcern.syncMode == WriteConcernOptions::SyncMode::UNSET);
}

enum class ReplyStyle { kUpdate, kNotUpdate };  // update has extra fields.
void serializeReply(OperationContext* opCtx,
                    ReplyStyle replyStyle,
                    bool continueOnError,
                    size_t opsInBatch,
                    const WriteResult& result,
                    BSONObjBuilder* out) {
    if (shouldSkipOutput(opCtx))
        return;

    long long n = 0;
    long long nModified = 0;
    std::vector<BSONObj> upsertInfo;
    std::vector<BSONObj> errors;
    BSONSizeTracker upsertInfoSizeTracker;
    BSONSizeTracker errorsSizeTracker;

    auto errorMessage = [&, errorSize = size_t(0) ](StringData rawMessage) mutable {
        // Start truncating error messages once both of these limits are exceeded.
        constexpr size_t kErrorSizeTruncationMin = 1024 * 1024;
        constexpr size_t kErrorCountTruncationMin = 2;
        if (errorSize >= kErrorSizeTruncationMin && errors.size() >= kErrorCountTruncationMin) {
            return ""_sd;
        }

        errorSize += rawMessage.size();
        return rawMessage;
    };

    for (size_t i = 0; i < result.results.size(); i++) {
        if (result.results[i].isOK()) {
            const auto& opResult = result.results[i].getValue();
            n += opResult.getN();  // Always there.
            if (replyStyle == ReplyStyle::kUpdate) {
                nModified += opResult.getNModified();
                if (auto idElement = opResult.getUpsertedId().firstElement()) {
                    BSONObjBuilder upsertedId(upsertInfoSizeTracker);
                    upsertedId.append("index", int(i));
                    upsertedId.appendAs(idElement, "_id");
                    upsertInfo.push_back(upsertedId.obj());
                }
            }
            continue;
        }

        const auto& status = result.results[i].getStatus();
        BSONObjBuilder error(errorsSizeTracker);
        error.append("index", int(i));
        error.append("code", int(status.code()));
        error.append("errmsg", errorMessage(status.reason()));
        errors.push_back(error.obj());
    }

	//写入有异常
    if (result.staleConfigException) {
        // For ordered:false commands we need to duplicate the StaleConfig result for all ops
        // after we stopped. result.results doesn't include the staleConfigException.
        // See the comment on WriteResult::staleConfigException for more info.
        int endIndex = continueOnError ? opsInBatch : result.results.size() + 1;
        for (int i = result.results.size(); i < endIndex; i++) {
            BSONObjBuilder error(errorsSizeTracker);
            error.append("index", i);
            error.append("code", int(ErrorCodes::StaleShardVersion));  // Different from exception!
            error.append("errmsg", errorMessage(result.staleConfigException->reason()));
            {
                BSONObjBuilder errInfo(error.subobjStart("errInfo"));
                result.staleConfigException->getVersionWanted().addToBSON(errInfo, "vWanted");
            }
            errors.push_back(error.obj());
        }
    }

    out->appendNumber("n", n);

    if (replyStyle == ReplyStyle::kUpdate) {
        out->appendNumber("nModified", nModified);
        if (!upsertInfo.empty()) {
            out->append("upserted", upsertInfo);
        }
    }

    if (!errors.empty()) {
        out->append("writeErrors", errors);
    }

    // writeConcernError field is handled by command processor.

    {
        // Undocumented repl fields that mongos depends on.
        auto* replCoord = repl::ReplicationCoordinator::get(opCtx->getServiceContext());
        const auto replMode = replCoord->getReplicationMode();
        if (replMode != repl::ReplicationCoordinator::modeNone) {
            const auto lastOp = repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp();
            if (lastOp.getTerm() == repl::OpTime::kUninitializedTerm) {
                out->append("opTime", lastOp.getTimestamp());
            } else {
                lastOp.append(out, "opTime");
            }

            if (replMode == repl::ReplicationCoordinator::modeReplSet) {
                out->append("electionId", replCoord->getElectionId());
            }
        }
    }
}

//mongod  WriteCommand(CmdInsert  CmdUpdate  CmdDelete等继承WriteCommand类,WriteCommand继承Command类)
//mongos  ClusterWriteCmd(ClusterCmdInsert  ClusterCmdUpdate  ClusterCmdDelete类继承该类，对应mongos转发)

//CmdInsert  CmdUpdate  CmdDelete等继承该类
class WriteCommand : public Command {
public:
    explicit WriteCommand(StringData name) : Command(name) {}

    bool slaveOk() const final {
        return false;
    }

    bool shouldAffectCommandCounter() const final {
        return false;
    }

    bool supportsWriteConcern(const BSONObj& cmd) const {
        return true;
    }

    ReadWriteType getReadWriteType() const {
        return ReadWriteType::kWrite;
    }
	//mongod(WriteCommand::enhancedRun(insert  delete update))
	//其他命令BasicCommand::enhancedRun  
	//mongos (ClusterWriteCmd::enhancedRun) 不同命令对应不同接口

    bool enhancedRun(OperationContext* opCtx,
                     const OpMsgRequest& request,
                     BSONObjBuilder& result) final {
        try {
			//CmdInsert::runImpl  CmdUpdate::runImpl   CmdDelete::runImpl
            runImpl(opCtx, request, result);
            return true;
        } catch (const DBException& ex) {
            LastError::get(opCtx->getClient()).setLastError(ex.code(), ex.reason());
            throw;
        }
    }

	//(CmdInsert  CmdUpdate  CmdDelete)::runImpl
    virtual void runImpl(OperationContext* opCtx,
                         const OpMsgRequest& request,
                         BSONObjBuilder& result) = 0;
};

/*例如插入过程会走CmdInsert::runImpl
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
//class CmdInsert final : public WriteCommand {yang add change
class CmdInsert : public WriteCommand { //
public:
    CmdInsert() : WriteCommand("insert") {}

    void redactForLogging(mutablebson::Document* cmdObj) final {
        redactTooLongLog(cmdObj, "documents");
    }

    void help(std::stringstream& help) const final {
        help << "insert documents";
    }

    Status checkAuthForRequest(OperationContext* opCtx, const OpMsgRequest& request) final {
        return checkAuthForWriteCommand(
            opCtx->getClient(), BatchedCommandRequest::BatchType_Insert, request);
    }

	//插入文档会走这里面  CmdInsert::runImpl
    void runImpl(OperationContext* opCtx,
                 const OpMsgRequest& request,
                 BSONObjBuilder& result) final {
        //从request中解析出write_ops::Insert类成员信息
        const auto batch = InsertOp::parse(request);
        const auto reply = performInserts(opCtx, batch);
        serializeReply(opCtx,
                       ReplyStyle::kNotUpdate,
                       !batch.getWriteCommandBase().getOrdered(),
                       batch.getDocuments().size(),
                       reply,
                       &result);
    }
} cmdInsert;
//生效见CmdInsert::runImpl CmdUpdate::runImpl CmdDelete::runImpl
//class CmdUpdate final : public WriteCommand {yang add change
class CmdUpdate : public WriteCommand {
public:
    CmdUpdate() : WriteCommand("update") {}

    void redactForLogging(mutablebson::Document* cmdObj) final {
        redactTooLongLog(cmdObj, "updates");
    }

    void help(std::stringstream& help) const final {
        help << "update documents";
    }

    Status checkAuthForRequest(OperationContext* opCtx, const OpMsgRequest& request) final {
        return checkAuthForWriteCommand(
            opCtx->getClient(), BatchedCommandRequest::BatchType_Update, request);
    }

	//CmdUpdate::runImpl
    void runImpl(OperationContext* opCtx,
                 const OpMsgRequest& request,
                 BSONObjBuilder& result) final {
        //解析出write_ops::Update
        const auto batch = UpdateOp::parse(request);
        const auto reply = performUpdates(opCtx, batch);
        serializeReply(opCtx,
                       ReplyStyle::kUpdate,
                       !batch.getWriteCommandBase().getOrdered(),
                       batch.getUpdates().size(),
                       reply,
                       &result);
    }

    Status explain(OperationContext* opCtx,
                   const std::string& dbname,
                   const BSONObj& cmdObj,
                   ExplainOptions::Verbosity verbosity,
                   BSONObjBuilder* out) const final {
        const auto opMsgRequest(OpMsgRequest::fromDBAndBody(dbname, cmdObj));
        const auto batch = UpdateOp::parse(opMsgRequest);
        uassert(ErrorCodes::InvalidLength,
                "explained write batches must be of size 1",
                batch.getUpdates().size() == 1);

        UpdateLifecycleImpl updateLifecycle(batch.getNamespace());
        UpdateRequest updateRequest(batch.getNamespace());
        updateRequest.setLifecycle(&updateLifecycle);
        updateRequest.setQuery(batch.getUpdates()[0].getQ());
        updateRequest.setUpdates(batch.getUpdates()[0].getU());
        updateRequest.setCollation(write_ops::collationOf(batch.getUpdates()[0]));
        updateRequest.setArrayFilters(write_ops::arrayFiltersOf(batch.getUpdates()[0]));
        updateRequest.setMulti(batch.getUpdates()[0].getMulti());
        updateRequest.setUpsert(batch.getUpdates()[0].getUpsert());
        updateRequest.setYieldPolicy(PlanExecutor::YIELD_AUTO);
        updateRequest.setExplain();

        ParsedUpdate parsedUpdate(opCtx, &updateRequest);
        uassertStatusOK(parsedUpdate.parseRequest());

        // Explains of write commands are read-only, but we take write locks so that timing
        // info is more accurate.
        AutoGetCollection collection(opCtx, batch.getNamespace(), MODE_IX);

        auto exec = uassertStatusOK(getExecutorUpdate(
            opCtx, &CurOp::get(opCtx)->debug(), collection.getCollection(), &parsedUpdate));
        Explain::explainStages(exec.get(), collection.getCollection(), verbosity, out);
        return Status::OK();
    }
} cmdUpdate;

//class CmdDelete final : public WriteCommand { //yang add change
class CmdDelete : public WriteCommand { //db.xx.remove操作也是转换为delete执行的
public:
    CmdDelete() : WriteCommand("delete") {}

    void redactForLogging(mutablebson::Document* cmdObj) final {
        redactTooLongLog(cmdObj, "deletes");
    }

    void help(std::stringstream& help) const final {
        help << "delete documents";
    }

    Status checkAuthForRequest(OperationContext* opCtx, const OpMsgRequest& request) final {
        return checkAuthForWriteCommand(
            opCtx->getClient(), BatchedCommandRequest::BatchType_Delete, request);
    }

	//CmdDelete::runImpl
    void runImpl(OperationContext* opCtx,
                 const OpMsgRequest& request,
                 BSONObjBuilder& result) final {
        //从request中解析出write_ops::Delete结构
        const auto batch = DeleteOp::parse(request);
        const auto reply = performDeletes(opCtx, batch);
        serializeReply(opCtx,
                       ReplyStyle::kNotUpdate,
                       !batch.getWriteCommandBase().getOrdered(),
                       batch.getDeletes().size(),
                       reply,
                       &result);
    }

    Status explain(OperationContext* opCtx,
                   const std::string& dbname,
                   const BSONObj& cmdObj,
                   ExplainOptions::Verbosity verbosity,
                   BSONObjBuilder* out) const final {
        const auto opMsgRequest(OpMsgRequest::fromDBAndBody(dbname, cmdObj));
        const auto batch = DeleteOp::parse(opMsgRequest);
        uassert(ErrorCodes::InvalidLength,
                "explained write batches must be of size 1",
                batch.getDeletes().size() == 1);

        DeleteRequest deleteRequest(batch.getNamespace());
        deleteRequest.setQuery(batch.getDeletes()[0].getQ());
        deleteRequest.setCollation(write_ops::collationOf(batch.getDeletes()[0]));
        deleteRequest.setMulti(batch.getDeletes()[0].getMulti());
        deleteRequest.setYieldPolicy(PlanExecutor::YIELD_AUTO);
        deleteRequest.setExplain();

        ParsedDelete parsedDelete(opCtx, &deleteRequest);
        uassertStatusOK(parsedDelete.parseRequest());

        // Explains of write commands are read-only, but we take write locks so that timing
        // info is more accurate.
        AutoGetCollection collection(opCtx, batch.getNamespace(), MODE_IX);

        // Explain the plan tree.
        auto exec = uassertStatusOK(getExecutorDelete(
            opCtx, &CurOp::get(opCtx)->debug(), collection.getCollection(), &parsedDelete));
        Explain::explainStages(exec.get(), collection.getCollection(), verbosity, out);
        return Status::OK();
    }
} cmdDelete;

}  // namespace
}  // namespace mongo
