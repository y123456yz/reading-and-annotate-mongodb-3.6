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
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kWrite

#include "mongo/platform/basic.h"

#include <memory>

#include "mongo/base/checked_cast.h"
#include "mongo/db/audit.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/database_holder.h"
#include "mongo/db/catalog/document_validation.h"
#include "mongo/db/commands.h"
#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/curop_metrics.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/exec/delete.h"
#include "mongo/db/exec/update.h"
#include "mongo/db/introspect.h"
#include "mongo/db/lasterror.h"
#include "mongo/db/ops/delete_request.h"
#include "mongo/db/ops/insert.h"
#include "mongo/db/ops/parsed_delete.h"
#include "mongo/db/ops/parsed_update.h"
#include "mongo/db/ops/update_lifecycle_impl.h"
#include "mongo/db/ops/update_request.h"
#include "mongo/db/ops/write_ops_exec.h"
#include "mongo/db/ops/write_ops_gen.h"
#include "mongo/db/ops/write_ops_retryability.h"
#include "mongo/db/query/get_executor.h"
#include "mongo/db/query/plan_summary_stats.h"
#include "mongo/db/query/query_knobs.h"
#include "mongo/db/repl/repl_client_info.h"
#include "mongo/db/repl/replication_coordinator.h"
#include "mongo/db/s/collection_sharding_state.h"
#include "mongo/db/s/sharding_state.h"
#include "mongo/db/session_catalog.h"
#include "mongo/db/stats/counters.h"
#include "mongo/db/stats/top.h"
#include "mongo/db/write_concern.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

// Convention in this file: generic helpers go in the anonymous namespace. Helpers that are for a
// single type of operation are static functions defined above their caller.
namespace {

MONGO_FP_DECLARE(failAllInserts);
MONGO_FP_DECLARE(failAllUpdates);
MONGO_FP_DECLARE(failAllRemoves);

//performUpdates   performDeletes
void finishCurOp(OperationContext* opCtx, CurOp* curOp) {
    try {
        curOp->done();
        long long executionTimeMicros =
            durationCount<Microseconds>(curOp->elapsedTimeExcludingPauses());
        curOp->debug().executionTimeMicros = executionTimeMicros;

        recordCurOpMetrics(opCtx);
        Top::get(opCtx->getServiceContext())
            .record(opCtx,
                    curOp->getNS(),
                    curOp->getLogicalOp(),
                    Top::LockType::WriteLocked,
                    durationCount<Microseconds>(curOp->elapsedTimeExcludingPauses()),
                    curOp->isCommand(),
                    curOp->getReadWriteType());

        if (!curOp->debug().exceptionInfo.isOK()) {
            LOG(3) << "Caught Assertion in " << redact(logicalOpToString(curOp->getLogicalOp()))
                   << ": " << curOp->debug().exceptionInfo.toString();
        }
		
        const bool logAll = logger::globalLogDomain()->shouldLog(logger::LogComponent::kCommand,
                                                                 logger::LogSeverity::Debug(1));
        const bool logSlow = executionTimeMicros > (serverGlobalParams.slowMS * 1000LL);
		
        const bool shouldSample = serverGlobalParams.sampleRate == 1.0
            ? true
            : opCtx->getClient()->getPrng().nextCanonicalDouble() < serverGlobalParams.sampleRate;
		
		//update和delete慢日志这里会记录一次，并且外层的ServiceEntryPointMongod::handleRequest还有记录一次
        if (logAll || (shouldSample && logSlow)) {//ServiceEntryPointMongod::handleRequest中也会有输出打印
            Locker::LockerInfo lockerInfo;
            opCtx->lockState()->getLockerInfo(&lockerInfo);

			//OpDebug::report
            log() << curOp->debug().report(opCtx->getClient(), *curOp, lockerInfo.stats);
        }

        if (curOp->shouldDBProfile(shouldSample)) {
            profile(opCtx, CurOp::get(opCtx)->getNetworkOp());
        }
    } catch (const DBException& ex) {
        // We need to ignore all errors here. We don't want a successful op to fail because of a
        // failure to record stats. We also don't want to replace the error reported for an op that
        // is failing.
        log() << "Ignoring error from finishCurOp: " << redact(ex);
    }
}

/**
 * Sets the Client's LastOp to the system OpTime if needed.
 */ //performInserts中构造使用
class LastOpFixer {
public:
    LastOpFixer(OperationContext* opCtx, const NamespaceString& ns)
        : _opCtx(opCtx), _isOnLocalDb(ns.isLocal()) {}

    ~LastOpFixer() {
        if (_needToFixLastOp && !_isOnLocalDb) {
            // If this operation has already generated a new lastOp, don't bother setting it
            // here. No-op updates will not generate a new lastOp, so we still need the
            // guard to fire in that case. Operations on the local DB aren't replicated, so they
            // don't need to bump the lastOp.
            replClientInfo().setLastOpToSystemLastOpTime(_opCtx);
        }
    }

    void startingOp() {
        _needToFixLastOp = true;
        _opTimeAtLastOpStart = replClientInfo().getLastOp();
    }

    void finishedOpSuccessfully() {
        // If the op was succesful and bumped LastOp, we don't need to do it again. However, we
        // still need to for no-ops and all failing ops.
        _needToFixLastOp = (replClientInfo().getLastOp() == _opTimeAtLastOpStart);
    }

private:
    repl::ReplClientInfo& replClientInfo() {
        return repl::ReplClientInfo::forClient(_opCtx->getClient());
    }

    OperationContext* const _opCtx;
    bool _needToFixLastOp = true;
    const bool _isOnLocalDb;
    repl::OpTime _opTimeAtLastOpStart;
};

void assertCanWrite_inlock(OperationContext* opCtx, const NamespaceString& ns) {
    uassert(ErrorCodes::PrimarySteppedDown,
            str::stream() << "Not primary while writing to " << ns.ns(),
            repl::ReplicationCoordinator::get(opCtx->getServiceContext())
                ->canAcceptWritesFor(opCtx, ns));
    CollectionShardingState::get(opCtx, ns)->checkShardVersionOrThrow(opCtx);
}

// 没有则创建集合及相关的索引文件 //insertBatchAndHandleErrors->makeCollection
void makeCollection(OperationContext* opCtx, const NamespaceString& ns) {
    writeConflictRetry(opCtx, "implicit collection creation", ns.ns(), [&opCtx, &ns] {
        AutoGetOrCreateDb db(opCtx, ns.db(), MODE_X);
        assertCanWrite_inlock(opCtx, ns);
		//Database::getCollection  判断该db下是否已经有对应的集合了，没有则创建
        if (!db.getDb()->getCollection(opCtx, ns)) {  // someone else may have beat us to it.
            WriteUnitOfWork wuow(opCtx);
			//创建新集合
            uassertStatusOK(userCreateNS(opCtx, db.getDb(), ns.ns(), BSONObj()));
            wuow.commit();
        }
    });
}

/**
 * Returns true if the operation can continue.
 */
bool handleError(OperationContext* opCtx,
                 const DBException& ex,
                 const NamespaceString& nss,
                 const write_ops::WriteCommandBase& wholeOp,
                 WriteResult* out) {
    LastError::get(opCtx->getClient()).setLastError(ex.code(), ex.reason());
    auto& curOp = *CurOp::get(opCtx);
    curOp.debug().exceptionInfo = ex.toStatus();

    if (ErrorCodes::isInterruption(ex.code())) {
        throw;  // These have always failed the whole batch.
    }

    if (ErrorCodes::isStaleShardingError(ex.code())) {
        auto staleConfigException = dynamic_cast<const StaleConfigException*>(&ex);
        if (!staleConfigException) {
            // We need to get extra info off of the SCE, but some common patterns can result in the
            // exception being converted to a Status then rethrown as a AssertionException, losing
            // the info we need. It would be a bug if this happens so we want to detect it in
            // testing, but it isn't severe enough that we should bring down the server if it
            // happens in production.
            dassert(staleConfigException);
            msgasserted(35475,
                        str::stream()
                            << "Got a StaleConfig error but exception was the wrong type: "
                            << demangleName(typeid(ex)));
        }

        if (!opCtx->getClient()->isInDirectClient()) {
            ShardingState::get(opCtx)
                ->onStaleShardVersion(opCtx, nss, staleConfigException->getVersionReceived())
                .transitional_ignore();
        }
        out->staleConfigException = stdx::make_unique<StaleConfigException>(*staleConfigException);
        return false;
    }

    out->results.emplace_back(ex.toStatus());

    return !wholeOp.getOrdered();
}

SingleWriteResult createIndex(OperationContext* opCtx,
                              const NamespaceString& systemIndexes,
                              const BSONObj& spec) {
    BSONElement nsElement = spec["ns"];
    uassert(ErrorCodes::NoSuchKey, "Missing \"ns\" field in index description", !nsElement.eoo());
    uassert(ErrorCodes::TypeMismatch,
            str::stream() << "Expected \"ns\" field of index description to be a "
                             "string, "
                             "but found a "
                          << typeName(nsElement.type()),
            nsElement.type() == String);
    const NamespaceString ns(nsElement.valueStringData());
    uassert(ErrorCodes::InvalidOptions,
            str::stream() << "Cannot create an index on " << ns.ns() << " with an insert to "
                          << systemIndexes.ns(),
            ns.db() == systemIndexes.db());

    BSONObjBuilder cmdBuilder;
    cmdBuilder << "createIndexes" << ns.coll();
    cmdBuilder << "indexes" << BSON_ARRAY(spec);

    auto cmdResult = Command::runCommandDirectly(
        opCtx, OpMsgRequest::fromDBAndBody(systemIndexes.db(), cmdBuilder.obj()));
    uassertStatusOK(getStatusFromCommandResult(cmdResult));

    // Unlike normal inserts, it is not an error to "insert" a duplicate index.
    long long n =
        cmdResult["numIndexesAfter"].numberInt() - cmdResult["numIndexesBefore"].numberInt();
    CurOp::get(opCtx)->debug().ninserted += n;

    SingleWriteResult result;
    result.setN(n);
    return result;
}

WriteResult performCreateIndexes(OperationContext* opCtx, const write_ops::Insert& wholeOp) {
    // Currently this creates each index independently. We could pass multiple indexes to
    // createIndexes, but there is a lot of complexity involved in doing it correctly. For one
    // thing, createIndexes only takes indexes to a single collection, but this batch could include
    // different collections. Additionally, the error handling is different: createIndexes is
    // all-or-nothing while inserts are supposed to behave like a sequence that either skips over
    // errors or stops at the first one. These could theoretically be worked around, but it doesn't
    // seem worth it since users that want faster index builds should just use the createIndexes
    // command rather than a legacy emulation.
    LastOpFixer lastOpFixer(opCtx, wholeOp.getNamespace());
    WriteResult out;
    for (auto&& spec : wholeOp.getDocuments()) {
        try {
            lastOpFixer.startingOp();
            out.results.emplace_back(createIndex(opCtx, wholeOp.getNamespace(), spec));
            lastOpFixer.finishedOpSuccessfully();
        } catch (const DBException& ex) {
            const bool canContinue =
                handleError(opCtx, ex, wholeOp.getNamespace(), wholeOp.getWriteCommandBase(), &out);
            if (!canContinue)
                break;
        }
    }
    return out;
}

//只有固定集合才会一次性多条文档进来，参考insertBatchAndHandleErrors,普通集合一次只会携带一个document进来
//insertBatchAndHandleErrors中调用执行
void insertDocuments(OperationContext* opCtx,
                     Collection* collection,
                     std::vector<InsertStatement>::iterator begin,
                     std::vector<InsertStatement>::iterator end) {
    // Intentionally not using writeConflictRetry. That is handled by the caller so it can react to
    // oversized batches.  执行一个写操作的事务 参考http://www.mongoing.com/archives/5476
    WriteUnitOfWork wuow(opCtx);

    // Acquire optimes and fill them in for each item in the batch.
    // This must only be done for doc-locking storage engines, which are allowed to insert oplog
    // documents out-of-timestamp-order.  For other storage engines, the oplog entries must be
    // physically written in timestamp order, so we defer optime assignment until the oplog is about
    // to be written.
    auto batchSize = std::distance(begin, end);
    if (supportsDocLocking()) {////wiredtiger是支持的，见 WiredTigerKVEngine::supportsDocLocking
        auto replCoord = repl::ReplicationCoordinator::get(opCtx);
        if (!replCoord->isOplogDisabledFor(opCtx, collection->ns())) {
            // Populate 'slots' with new optimes for each insert.
            // This also notifies the storage engine of each new timestamp.
            auto oplogSlots = repl::getNextOpTimes(opCtx, batchSize);
            auto slot = oplogSlots.begin();
            for (auto it = begin; it != end; it++) {
                it->oplogSlot = *slot++;
            }
        }
    }

	//CollectionImpl::insertDocuments   
    uassertStatusOK(collection->insertDocuments(
        opCtx, begin, end, &CurOp::get(opCtx)->debug(), /*enforceQuota*/ true));
    wuow.commit(); //WriteUnitOfWork::commit
}

/**
 * Returns true if caller should try to insert more documents. Does nothing else if batch is empty.
 */ //performInserts中调用
bool insertBatchAndHandleErrors(OperationContext* opCtx,
                                const write_ops::Insert& wholeOp,
                                std::vector<InsertStatement>& batch,
                                LastOpFixer* lastOpFixer,
                                WriteResult* out) {
    if (batch.empty())
        return true;

    auto& curOp = *CurOp::get(opCtx);

    boost::optional<AutoGetCollection> collection;
    auto acquireCollection = [&] { //创建集合文件 索引文件等
        while (true) {
            opCtx->checkForInterrupt();

            if (MONGO_FAIL_POINT(failAllInserts)) {
                uasserted(ErrorCodes::InternalError, "failAllInserts failpoint active!");
            }

			//通过这里最终调用AutoGetCollection构造函数，锁构造初始化也在这里
			//根据表名构造collection
            collection.emplace(opCtx, wholeOp.getNamespace(), MODE_IX);
			//AutoGetCollection::getCollection
            if (collection->getCollection()) //已经有该集合了
                break;

            collection.reset();  //    没有则创建集合及相关的索引文件
            makeCollection(opCtx, wholeOp.getNamespace()); 
        }

        curOp.raiseDbProfileLevel(collection->getDb()->getProfilingLevel());
        assertCanWrite_inlock(opCtx, wholeOp.getNamespace());
    };

    try {
        acquireCollection(); //执行上面定义的函数
        //MongoDB 固定集合（Capped Collections）是性能出色且有着固定大小的集合，对于大小固定，
        //我们可以想象其就像一个环形队列，当集合空间用完后，再插入的元素就会覆盖最初始的头部的元素！
        if (!collection->getCollection()->isCapped() && batch.size() > 1) { //如果是固定集合，一次性插入，在同一个事务中，见insertDocuments
            // First try doing it all together. If all goes well, this is all we need to do.
            // See Collection::_insertDocuments for why we do all capped inserts one-at-a-time.
            lastOpFixer->startingOp();
            insertDocuments(opCtx, collection->getCollection(), batch.begin(), batch.end());
            lastOpFixer->finishedOpSuccessfully();
            globalOpCounters.gotInserts(batch.size());
            SingleWriteResult result;
            result.setN(1);

            std::fill_n(std::back_inserter(out->results), batch.size(), std::move(result));
            curOp.debug().ninserted += batch.size();
            return true;
        }
    } catch (const DBException&) {
        collection.reset();

        // Ignore this failure and behave as-if we never tried to do the combined batch insert.
        // The loop below will handle reporting any non-transient errors.
    }

    // Try to insert the batch one-at-a-time. This path is executed both for singular batches, and
    // for batches that failed all-at-once inserting.
    //一次性一条一条插入，上面的固定集合是一次性插入
    //log() << "yang test ...insertBatchAndHandleErrors.........getNamespace().ns():" << wholeOp.getNamespace().ns();
    for (auto it = batch.begin(); it != batch.end(); ++it) {
        globalOpCounters.gotInsert(); //insert操作计数
        try {
			//log() << "yang test ............getNamespace().ns():" << wholeOp.getNamespace().ns();
			//writeConflictRetry里面会执行{}中的函数体
            writeConflictRetry(opCtx, "insert", wholeOp.getNamespace().ns(), [&] {
                try {
                    if (!collection)
                        acquireCollection(); //执行上面定义的函数  创建集合
                    lastOpFixer->startingOp(); //记录本次操作的时间
                    //把该条文档插入  
                    insertDocuments(opCtx, collection->getCollection(), it, it + 1);
                    lastOpFixer->finishedOpSuccessfully();
                    SingleWriteResult result;
                    result.setN(1);
                    out->results.emplace_back(std::move(result));
                    curOp.debug().ninserted++;
                } catch (...) {
                    // Release the lock following any error. Among other things, this ensures that
                    // we don't sleep in the WCE retry loop with the lock held.
                    collection.reset();
                    throw;
                }
            });
        } catch (const DBException& ex) {
            bool canContinue =
                handleError(opCtx, ex, wholeOp.getNamespace(), wholeOp.getWriteCommandBase(), out);
            if (!canContinue)
                return false;
        }
    }

    return true;
}

//performInserts调用
template <typename T>
StmtId getStmtIdForWriteOp(OperationContext* opCtx, const T& wholeOp, size_t opIndex) {
    return opCtx->getTxnNumber() ? write_ops::getStmtIdForWriteAt(wholeOp, opIndex)
                                 : kUninitializedStmtId;
}

SingleWriteResult makeWriteResultForInsertOrDeleteRetry() {
    SingleWriteResult res;
    res.setN(1);
    res.setNModified(0);
    return res;
}

}  // namespace

//以前老版本receivedInsert中调用，3.6新版本在CmdInsert::runImpl中调用
//performDeletes(CmdDelete::runImpl)  performUpdates(CmdUpdate::runImpl)  performInserts(CmdInsert::runImpl)分别对应删除、更新、插入
//把insert的一批数据按照单次最多64个文档，最大256K字节拆分为多个batch，然后调用insertBatchAndHandleErrors处理
WriteResult performInserts(OperationContext* opCtx, const write_ops::Insert& wholeOp) {
    invariant(!opCtx->lockState()->inAWriteUnitOfWork());  // Does own retries.
    auto& curOp = *CurOp::get(opCtx);
	 //ScopeGuard scopeGuard$line = MakeGuard([&] {   });
    ON_BLOCK_EXIT([&] {
        // This is the only part of finishCurOp we need to do for inserts because they reuse the
        // top-level curOp. The rest is handled by the top-level entrypoint.
        //performInserts函数执行完成后，需要调用该函数
        curOp.done(); //performInserts执行完成后调用，记录执行结束时间    
        Top::get(opCtx->getServiceContext())
            .record(opCtx,
                    wholeOp.getNamespace().ns(),
                    LogicalOp::opInsert,
                    Top::LockType::WriteLocked,
                    durationCount<Microseconds>(curOp.elapsedTimeExcludingPauses()),
                    curOp.isCommand(),
                    curOp.getReadWriteType());

    });

    {
        stdx::lock_guard<Client> lk(*opCtx->getClient()); //该链接对应客户端上锁
        curOp.setNS_inlock(wholeOp.getNamespace().ns()); //设置当前操作的集合名
        curOp.setLogicalOp_inlock(LogicalOp::opInsert); //设置操作类型
        curOp.ensureStarted(); //设置该操作开始时间
        curOp.debug().ninserted = 0;
    }

	//log() << "yang test ................... getnamespace:" << wholeOp.getNamespace() << wholeOp.getDbName();
	//例如use test;db.test.insert({"yang":1, "ya":2}),则_nss为test.test, _dbname为test
	uassertStatusOK(userAllowedWriteNS(wholeOp.getNamespace())); //对集合做检查
	
    if (wholeOp.getNamespace().isSystemDotIndexes()) { 
		//3.6.3开始的版本，不会有system.index库了，index而是记录到_mdb_catalog.wt，所以这里永远不会进来
        return performCreateIndexes(opCtx, wholeOp);
    }

    DisableDocumentValidationIfTrue docValidationDisabler(
        opCtx, wholeOp.getWriteCommandBase().getBypassDocumentValidation());
    LastOpFixer lastOpFixer(opCtx, wholeOp.getNamespace());

    WriteResult out;
    out.results.reserve(wholeOp.getDocuments().size());

    size_t stmtIdIndex = 0;
    size_t bytesInBatch = 0;
    std::vector<InsertStatement> batch; //数组
    //默认64
    const size_t maxBatchSize = internalInsertMaxBatchSize.load();
	//确定InsertStatement类型数组的总长度，这里默认一次性最多批量处理64个documents
    batch.reserve(std::min(wholeOp.getDocuments().size(), maxBatchSize));

    for (auto&& doc : wholeOp.getDocuments()) {
		//是否wholeOp中从网络接收到的最后一个document(例如客户端一次性insert多个doc，则这里就是确定是否是最后一个doc)
        const bool isLastDoc = (&doc == &wholeOp.getDocuments().back());
		//对doc文档做检查，返回新的BSONObj
		//这里面会遍历所有的bson成员elem，并做相应的检查，并给该doc添加相应的ID
        auto fixedDoc = fixDocumentForInsert(opCtx->getServiceContext(), doc);
        if (!fixedDoc.isOK()) { //如果这个文档检测有异常，则跳过这个文档，进行下一个文档操作
            // Handled after we insert anything in the batch to be sure we report errors in the
            // correct order. In an ordered insert, if one of the docs ahead of us fails, we should
            // behave as-if we never got to this document.
        } else {
        	//获取stdtid
            const auto stmtId = getStmtIdForWriteOp(opCtx, wholeOp, stmtIdIndex++);
            if (opCtx->getTxnNumber()) {
                auto session = OperationContextSession::get(opCtx);
                if (session->checkStatementExecutedNoOplogEntryFetch(*opCtx->getTxnNumber(),
                                                                     stmtId)) {
                    out.results.emplace_back(makeWriteResultForInsertOrDeleteRetry());
                    continue;
                }
            }

            BSONObj toInsert = fixedDoc.getValue().isEmpty() ? doc : std::move(fixedDoc.getValue());
			// db.collname.insert({"name":"yangyazhou1", "age":22})
			//yang test performInserts... doc:{ _id: ObjectId('5badf00412ee982ae019e0c1'), name: "yangyazhou1", age: 22.0 }
			//log() << "yang test performInserts... doc:" << redact(toInsert);
			//把文档插入到batch数组
            batch.emplace_back(stmtId, toInsert);
            bytesInBatch += batch.back().doc.objsize();
			//这里continue，就是为了把批量插入的文档组成到一个batch数组中，到达一定量一次性插入

			//batch里面一次最多插入64个文档，总字节数不超过256K
            if (!isLastDoc && batch.size() < maxBatchSize && bytesInBatch < insertVectorMaxBytes)
                continue;  // Add more to batch before inserting.
        }

		//把batch数组中的doc文档写入存储引擎
        bool canContinue = insertBatchAndHandleErrors(opCtx, wholeOp, batch, &lastOpFixer, &out);
        batch.clear();  // We won't need the current batch any more.
        bytesInBatch = 0;

        if (canContinue && !fixedDoc.isOK()) {
			//insert统计计数
            globalOpCounters.gotInsert();
            try {
                uassertStatusOK(fixedDoc.getStatus());
                MONGO_UNREACHABLE;
            } catch (const DBException& ex) {
                canContinue = handleError(
                    opCtx, ex, wholeOp.getNamespace(), wholeOp.getWriteCommandBase(), &out);
            }
        }

        if (!canContinue)
            break;
    }

    return out;
}

//performUpdates中调用
static SingleWriteResult performSingleUpdateOp(OperationContext* opCtx,
                                               const NamespaceString& ns,
                                               StmtId stmtId,
                                               const write_ops::UpdateOpEntry& op) {
	//是否可重试可以参考https://www.docs4dev.com/docs/zh/mongodb/v3.6/reference/core-retryable-writes.html#enabling-retryable-writes
	uassert(ErrorCodes::InvalidOptions,
            "Cannot use (or request) retryable writes with multi=true",
            !(opCtx->getTxnNumber() && op.getMulti()));

	//update操作统计
    globalOpCounters.gotUpdate();
    auto& curOp = *CurOp::get(opCtx);
    {
        stdx::lock_guard<Client> lk(*opCtx->getClient());
        curOp.setNS_inlock(ns.ns());
        curOp.setNetworkOp_inlock(dbUpdate);
        curOp.setLogicalOp_inlock(LogicalOp::opUpdate);
        curOp.setOpDescription_inlock(op.toBSON());
		//记录开始时间
        curOp.ensureStarted();
    }

    UpdateLifecycleImpl updateLifecycle(ns);

	//根据op ns生成UpdateRequest
    UpdateRequest request(ns);
	//UpdateRequest::setLifecycle  设置update生命周期
    request.setLifecycle(&updateLifecycle);
	//查询条件
    request.setQuery(op.getQ());
	//更新内容
    request.setUpdates(op.getU());
	//UpdateOpEntry::getCollation
	//Collation特性允许MongoDB的用户根据不同的语言定制排序规则 https://mongoing.com/archives/3912
    request.setCollation(write_ops::collationOf(op));
	//stmtId设置
    request.setStmtId(stmtId);
    request.setArrayFilters(write_ops::arrayFiltersOf(op));
    request.setMulti(op.getMulti());
    request.setUpsert(op.getUpsert());
    request.setYieldPolicy(PlanExecutor::YIELD_AUTO);  // ParsedUpdate overrides this for $isolated.

    ParsedUpdate parsedUpdate(opCtx, &request);
    uassertStatusOK(parsedUpdate.parseRequest());

    boost::optional<AutoGetCollection> collection;
    while (true) {
        opCtx->checkForInterrupt();
        if (MONGO_FAIL_POINT(failAllUpdates)) {
            uasserted(ErrorCodes::InternalError, "failAllUpdates failpoint active!");
        }

        collection.emplace(opCtx,
                           ns,
                           MODE_IX,  // DB is always IX, even if collection is X.
                           parsedUpdate.isIsolated() ? MODE_X : MODE_IX);
        if (collection->getCollection() || !op.getUpsert())
            break;

        collection.reset();  // unlock.
        makeCollection(opCtx, ns);
    }

    if (collection->getDb()) {
        curOp.raiseDbProfileLevel(collection->getDb()->getProfilingLevel());
    }

    assertCanWrite_inlock(opCtx, ns);

    auto exec = uassertStatusOK(
        getExecutorUpdate(opCtx, &curOp.debug(), collection->getCollection(), &parsedUpdate));

    {
        stdx::lock_guard<Client> lk(*opCtx->getClient());
        CurOp::get(opCtx)->setPlanSummary_inlock(Explain::getPlanSummary(exec.get()));
    }

    uassertStatusOK(exec->executePlan());

    PlanSummaryStats summary;
    Explain::getSummaryStats(*exec, &summary);
    if (collection->getCollection()) {
        collection->getCollection()->infoCache()->notifyOfQuery(opCtx, summary.indexesUsed);
    }

    if (curOp.shouldDBProfile()) {
        BSONObjBuilder execStatsBob;
        Explain::getWinningPlanStats(exec.get(), &execStatsBob);
        curOp.debug().execStats = execStatsBob.obj();
    }

    const UpdateStats* updateStats = UpdateStage::getUpdateStats(exec.get());
    UpdateStage::recordUpdateStatsInOpDebug(updateStats, &curOp.debug());
    curOp.debug().setPlanSummaryMetrics(summary);
    UpdateResult res = UpdateStage::makeUpdateResult(updateStats);

    const bool didInsert = !res.upserted.isEmpty();
    const long long nMatchedOrInserted = didInsert ? 1 : res.numMatched;
    LastError::get(opCtx->getClient()).recordUpdate(res.existing, nMatchedOrInserted, res.upserted);

    SingleWriteResult result;
    result.setN(nMatchedOrInserted);
    result.setNModified(res.numDocsModified);
    result.setUpsertedId(res.upserted);

    return result;
}

//performDeletes(CmdDelete::runImpl)  performUpdates(CmdUpdate::runImpl)  performInserts(CmdInsert::runImpl)
WriteResult performUpdates(OperationContext* opCtx, const write_ops::Update& wholeOp) {
    invariant(!opCtx->lockState()->inAWriteUnitOfWork());  // Does own retries.
    //检查是否可以对ns进行写操作，有些内部ns是不能写的
    uassertStatusOK(userAllowedWriteNS(wholeOp.getNamespace()));

    DisableDocumentValidationIfTrue docValidationDisabler(
        opCtx, wholeOp.getWriteCommandBase().getBypassDocumentValidation());
    LastOpFixer lastOpFixer(opCtx, wholeOp.getNamespace());

    size_t stmtIdIndex = 0;
    WriteResult out;
    out.results.reserve(wholeOp.getUpdates().size());

	//write_ops::Update::getUpdates    singleOp为UpdateOpEntry类型
    for (auto&& singleOp : wholeOp.getUpdates()) {
        const auto stmtId = getStmtIdForWriteOp(opCtx, wholeOp, stmtIdIndex++);
        if (opCtx->getTxnNumber()) {
            auto session = OperationContextSession::get(opCtx);
            if (auto entry =
                    session->checkStatementExecuted(opCtx, *opCtx->getTxnNumber(), stmtId)) {
                out.results.emplace_back(parseOplogEntryForUpdate(*entry));
                continue;
            }
        }

        // TODO: don't create nested CurOp for legacy writes.
        // Add Command pointer to the nested CurOp.
        auto& parentCurOp = *CurOp::get(opCtx);
        Command* cmd = parentCurOp.getCommand();
        CurOp curOp(opCtx);
        {
            stdx::lock_guard<Client> lk(*opCtx->getClient());
            curOp.setCommand_inlock(cmd);
        }
        ON_BLOCK_EXIT([&] { finishCurOp(opCtx, &curOp); }); //计算整个操作消耗的时间
        try {
			//LastOpFixer::startingOp
            lastOpFixer.startingOp();
            out.results.emplace_back(
                performSingleUpdateOp(opCtx, wholeOp.getNamespace(), stmtId, singleOp));
            lastOpFixer.finishedOpSuccessfully();
        } catch (const DBException& ex) {
            const bool canContinue =
                handleError(opCtx, ex, wholeOp.getNamespace(), wholeOp.getWriteCommandBase(), &out);
            if (!canContinue)
                break;
        }
    }

    return out;
}

static SingleWriteResult performSingleDeleteOp(OperationContext* opCtx,
                                               const NamespaceString& ns,
                                               StmtId stmtId,
                                               const write_ops::DeleteOpEntry& op) {
    uassert(ErrorCodes::InvalidOptions,
            "Cannot use (or request) retryable writes with limit=0",
            !(opCtx->getTxnNumber() && op.getMulti()));

    globalOpCounters.gotDelete();
    auto& curOp = *CurOp::get(opCtx);
    {
        stdx::lock_guard<Client> lk(*opCtx->getClient());
        curOp.setNS_inlock(ns.ns());
        curOp.setNetworkOp_inlock(dbDelete);
        curOp.setLogicalOp_inlock(LogicalOp::opDelete);
        curOp.setOpDescription_inlock(op.toBSON());
        curOp.ensureStarted();
    }

    curOp.debug().ndeleted = 0;

    DeleteRequest request(ns);
    request.setQuery(op.getQ());
    request.setCollation(write_ops::collationOf(op));
    request.setMulti(op.getMulti());
    request.setYieldPolicy(PlanExecutor::YIELD_AUTO);  // ParsedDelete overrides this for $isolated.
    request.setStmtId(stmtId);

    ParsedDelete parsedDelete(opCtx, &request);
    uassertStatusOK(parsedDelete.parseRequest());

    opCtx->checkForInterrupt();

    if (MONGO_FAIL_POINT(failAllRemoves)) {
        uasserted(ErrorCodes::InternalError, "failAllRemoves failpoint active!");
    }

    AutoGetCollection collection(opCtx,
                                 ns,
                                 MODE_IX,  // DB is always IX, even if collection is X.
                                 parsedDelete.isIsolated() ? MODE_X : MODE_IX);
    if (collection.getDb()) {
        curOp.raiseDbProfileLevel(collection.getDb()->getProfilingLevel());
    }

    assertCanWrite_inlock(opCtx, ns);

    auto exec = uassertStatusOK(
        getExecutorDelete(opCtx, &curOp.debug(), collection.getCollection(), &parsedDelete));

    {
        stdx::lock_guard<Client> lk(*opCtx->getClient());
        CurOp::get(opCtx)->setPlanSummary_inlock(Explain::getPlanSummary(exec.get()));
    }

    uassertStatusOK(exec->executePlan());
    long long n = DeleteStage::getNumDeleted(*exec);
    curOp.debug().ndeleted = n;

    PlanSummaryStats summary;
    Explain::getSummaryStats(*exec, &summary);
    if (collection.getCollection()) {
        collection.getCollection()->infoCache()->notifyOfQuery(opCtx, summary.indexesUsed);
    }
    curOp.debug().setPlanSummaryMetrics(summary);

    if (curOp.shouldDBProfile()) {
        BSONObjBuilder execStatsBob;
        Explain::getWinningPlanStats(exec.get(), &execStatsBob);
        curOp.debug().execStats = execStatsBob.obj();
    }

    LastError::get(opCtx->getClient()).recordDelete(n);

    SingleWriteResult result;
    result.setN(n);
    return result;
}

//performDeletes(CmdDelete::runImpl)  performUpdates(CmdUpdate::runImpl)  performInserts(CmdInsert::runImpl)
WriteResult performDeletes(OperationContext* opCtx, const write_ops::Delete& wholeOp) {
    invariant(!opCtx->lockState()->inAWriteUnitOfWork());  // Does own retries.
    uassertStatusOK(userAllowedWriteNS(wholeOp.getNamespace()));

    DisableDocumentValidationIfTrue docValidationDisabler(
        opCtx, wholeOp.getWriteCommandBase().getBypassDocumentValidation());
    LastOpFixer lastOpFixer(opCtx, wholeOp.getNamespace());

    size_t stmtIdIndex = 0;
    WriteResult out;
    out.results.reserve(wholeOp.getDeletes().size());
	log() << "yang test ........................ performDeletes";

    for (auto&& singleOp : wholeOp.getDeletes()) {
        const auto stmtId = getStmtIdForWriteOp(opCtx, wholeOp, stmtIdIndex++);
        if (opCtx->getTxnNumber()) {
            auto session = OperationContextSession::get(opCtx);
            if (session->checkStatementExecutedNoOplogEntryFetch(*opCtx->getTxnNumber(), stmtId)) {
                out.results.emplace_back(makeWriteResultForInsertOrDeleteRetry());
                continue;
            }
        }

        // TODO: don't create nested CurOp for legacy writes.
        // Add Command pointer to the nested CurOp.
        auto& parentCurOp = *CurOp::get(opCtx);
        Command* cmd = parentCurOp.getCommand();
        CurOp curOp(opCtx);
        {
            stdx::lock_guard<Client> lk(*opCtx->getClient());
            curOp.setCommand_inlock(cmd);
        }
        ON_BLOCK_EXIT([&] { finishCurOp(opCtx, &curOp); });
        try {
            lastOpFixer.startingOp();
            out.results.emplace_back(
                performSingleDeleteOp(opCtx, wholeOp.getNamespace(), stmtId, singleOp));
            lastOpFixer.finishedOpSuccessfully();
        } catch (const DBException& ex) {
            const bool canContinue =
                handleError(opCtx, ex, wholeOp.getNamespace(), wholeOp.getWriteCommandBase(), &out);
            if (!canContinue)
                break;
        }
    }

    return out;
}

}  // namespace mongo
