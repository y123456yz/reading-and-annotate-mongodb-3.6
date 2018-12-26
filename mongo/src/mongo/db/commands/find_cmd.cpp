/**
 *    Copyright (C) 2014 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kQuery

#include "mongo/platform/basic.h"

#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/client.h"
#include "mongo/db/clientcursor.h"
#include "mongo/db/commands.h"
#include "mongo/db/commands/run_aggregate.h"
#include "mongo/db/concurrency/d_concurrency.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/exec/working_set_common.h"
#include "mongo/db/matcher/extensions_callback_real.h"
#include "mongo/db/query/cursor_response.h"
#include "mongo/db/query/explain.h"
#include "mongo/db/query/find.h"
#include "mongo/db/query/find_common.h"
#include "mongo/db/query/get_executor.h"
#include "mongo/db/repl/replication_coordinator.h"
#include "mongo/db/s/collection_sharding_state.h"
#include "mongo/db/server_parameters.h"
#include "mongo/db/service_context.h"
#include "mongo/db/stats/counters.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/util/log.h"

namespace mongo {
namespace {

const auto kTermField = "term"_sd;

/**
 * A command for running .find() queries.
 */
class FindCmd : public BasicCommand {
public:
    FindCmd() : BasicCommand("find") {}

    bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }

    bool slaveOk() const override {
        return false;
    }

    bool slaveOverrideOk() const override {
        return true;
    }

    bool maintenanceOk() const override {
        return false;
    }

    bool adminOnly() const override {
        return false;
    }

    bool supportsNonLocalReadConcern(const std::string& dbName, const BSONObj& cmdObj) const final {
        return true;
    }

    void help(std::stringstream& help) const override {
        help << "query for documents";
    }

    LogicalOp getLogicalOp() const override {
        return LogicalOp::opQuery;
    }

    ReadWriteType getReadWriteType() const override {
        return ReadWriteType::kRead;
    }

    std::size_t reserveBytesForReply() const override {
        return FindCommon::kInitReplyBufferSize;
    }

    /**
     * A find command does not increment the command counter, but rather increments the
     * query counter.
     */
    bool shouldAffectCommandCounter() const override {
        return false;
    }

    Status checkAuthForOperation(OperationContext* opCtx,
                                 const std::string& dbname,
                                 const BSONObj& cmdObj) override {
        AuthorizationSession* authSession = AuthorizationSession::get(opCtx->getClient());

        if (!authSession->isAuthorizedToParseNamespaceElement(cmdObj.firstElement())) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }
        const NamespaceString nss(parseNsOrUUID(opCtx, dbname, cmdObj));
        auto hasTerm = cmdObj.hasField(kTermField);
        return authSession->checkAuthForFind(nss, hasTerm);
    }

    Status explain(OperationContext* opCtx,
                   const std::string& dbname,
                   const BSONObj& cmdObj,
                   ExplainOptions::Verbosity verbosity,
                   BSONObjBuilder* out) const override {
        const NamespaceString nss(parseNs(dbname, cmdObj));
        if (!nss.isValid()) {
            return {ErrorCodes::InvalidNamespace,
                    str::stream() << "Invalid collection name: " << nss.ns()};
        }

        // Parse the command BSON to a QueryRequest.
        const bool isExplain = true;
        auto qrStatus = QueryRequest::makeFromFindCommand(nss, cmdObj, isExplain);
        if (!qrStatus.isOK()) {
            return qrStatus.getStatus();
        }

        // Finish the parsing step by using the QueryRequest to create a CanonicalQuery.

        ExtensionsCallbackReal extensionsCallback(opCtx, &nss);
        const boost::intrusive_ptr<ExpressionContext> expCtx;
        auto statusWithCQ =
            CanonicalQuery::canonicalize(opCtx,
                                         std::move(qrStatus.getValue()),
                                         expCtx,
                                         extensionsCallback,
                                         MatchExpressionParser::kAllowAllSpecialFeatures &
                                             ~MatchExpressionParser::AllowedFeatures::kIsolated);
        if (!statusWithCQ.isOK()) {
            return statusWithCQ.getStatus();
        }
        std::unique_ptr<CanonicalQuery> cq = std::move(statusWithCQ.getValue());

        // Acquire locks. If the namespace is a view, we release our locks and convert the query
        // request into an aggregation command.
        AutoGetCollectionOrViewForReadCommand ctx(opCtx, nss);
        if (ctx.getView()) {
            // Relinquish locks. The aggregation command will re-acquire them.
            ctx.releaseLocksForView();

            // Convert the find command into an aggregation using $match (and other stages, as
            // necessary), if possible.
            const auto& qr = cq->getQueryRequest();
            auto viewAggregationCommand = qr.asAggregationCommand();
            if (!viewAggregationCommand.isOK())
                return viewAggregationCommand.getStatus();

            // Create the agg request equivalent of the find operation, with the explain verbosity
            // included.
            auto aggRequest = AggregationRequest::parseFromBSON(
                nss, viewAggregationCommand.getValue(), verbosity);
            if (!aggRequest.isOK()) {
                return aggRequest.getStatus();
            }

            try {
                return runAggregate(
                    opCtx, nss, aggRequest.getValue(), viewAggregationCommand.getValue(), *out);
            } catch (DBException& error) {
                if (error.code() == ErrorCodes::InvalidPipelineOperator) {
                    return {ErrorCodes::InvalidPipelineOperator,
                            str::stream() << "Unsupported in view pipeline: " << error.what()};
                }
                return error.toStatus();
            }
        }

        // The collection may be NULL. If so, getExecutor() should handle it by returning an
        // execution tree with an EOFStage.
        Collection* collection = ctx.getCollection();

        // We have a parsed query. Time to get the execution plan for it.
        auto statusWithPlanExecutor =
            getExecutorFind(opCtx, collection, nss, std::move(cq), PlanExecutor::YIELD_AUTO);
        if (!statusWithPlanExecutor.isOK()) {
            return statusWithPlanExecutor.getStatus();
        }
        auto exec = std::move(statusWithPlanExecutor.getValue());

        // Got the execution tree. Explain it.
        Explain::explainStages(exec.get(), collection, verbosity, out);
        return Status::OK();
    }

    /**
     * Runs a query using the following steps:
     *   --Parsing.
     *   --Acquire locks.
     *   --Plan query, obtaining an executor that can run it.
     *   --Generate the first batch.
     *   --Save state for getMore, transferring ownership of the executor to a ClientCursor.
     *   --Generate response to send to the client.
     */ 
	/*
	查询过程调用栈
#4  0x00007fc81c0a4613 in mongo::(anonymous namespace)::FindCmd::run (this=this@entry=0x7fc81e381740 <mongo::(anonymous namespace)::findCmd>, opCtx=opCtx@entry=0x7fc824b30dc0, dbname=..., cmdObj=..., result=...)
at src/mongo/db/commands/find_cmd.cpp:311
#5  0x00007fc81d0d7b36 in mongo::BasicCommand::enhancedRun (this=0x7fc81e381740 <mongo::(anonymous namespace)::findCmd>, opCtx=0x7fc824b30dc0, request=..., result=...) at src/mongo/db/commands.cpp:416
#6  0x00007fc81d0d42df in mongo::Command::publicRun (this=0x7fc81e381740 <mongo::(anonymous namespace)::findCmd>, opCtx=0x7fc824b30dc0, request=..., result=...) at src/mongo/db/commands.cpp:354
#7  0x00007fc81c0501f4 in runCommandImpl (startOperationTime=..., replyBuilder=0x7fc824ac1330, request=..., command=0x7fc81e381740 <mongo::(anonymous namespace)::findCmd>, opCtx=0x7fc824b30dc0)
    at src/mongo/db/service_entry_point_mongod.cpp:481
#8  mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7fc824b30dc0, command=command@entry=0x7fc81e381740 <mongo::(anonymous namespace)::findCmd>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:757
#9  0x00007fc81c05136f in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fc81b4a1400) at src/mongo/db/service_entry_point_mongod.cpp:878
#10 0x00007fc81c05136f in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#11 0x00007fc81c0521d1 in runCommands (message=..., opCtx=0x7fc824b30dc0) at src/mongo/db/service_entry_point_mongod.cpp:888
	*/
	/* 参考https://yq.aliyun.com/articles/647563?spm=a2c4e.11155435.0.0.7cb74df3gUVck4
	1). Query会进行简单的处理(标准化)，并构造一些上下文数据结构变成CanonicalQuery(标准化Query)。
	2). Plan模块会负责生成该Query的多个执行计划，然后丢给Optimizer去选择最优的，丢给PlanExecutor。
	3). PlanExecutor按照执行计划一步一步迭代，获得最终的数据(或执行update修改数据)。
	在此流程中：

	Plan如果只关联到单个或零个索引，这只生成一个执行计划，如果发现有多个索引或者索引有重叠，这可能生成多个执行计划。
	Optimizer只在多个执行计划时，才会介入。
	*/
	//也就是FindCmd::run，查询请求会走这里    查询过程可以参考https://yq.aliyun.com/articles/215016
    bool run(OperationContext* opCtx,  
             const std::string& dbname,
             const BSONObj& cmdObj,
             BSONObjBuilder& result) override {
        // Although it is a command, a find command gets counted as a query.
        globalOpCounters.gotQuery(); //query查询计数加1

        // Parse the command BSON to a QueryRequest.
        const bool isExplain = false;
        // Pass parseNs to makeFromFindCommand in case cmdObj does not have a UUID.
        //QueryRequest::makeFromFindCommand->parseFromFindCommand
        //解析并检查bson文档内容及格式
        auto qrStatus = QueryRequest::makeFromFindCommand(
            NamespaceString(parseNs(dbname, cmdObj)), cmdObj, isExplain);
        if (!qrStatus.isOK()) { //BSON报文内容格式是否符合要求
            return appendCommandStatus(result, qrStatus.getStatus());
        }

        auto& qr = qrStatus.getValue(); //也就是获取对应的QueryRequest

        // Validate term before acquiring locks, if provided.
        if (auto term = qr->getReplicationTerm()) {
            auto replCoord = repl::ReplicationCoordinator::get(opCtx);
            Status status = replCoord->updateTerm(opCtx, *term);
            // Note: updateTerm returns ok if term stayed the same.
            if (!status.isOK()) {
                return appendCommandStatus(result, status);
            }
        }

        // Acquire locks. If the query is on a view, we release our locks and convert the query
        // request into an aggregation command.
        Lock::DBLock dbSLock(opCtx, dbname, MODE_IS);
        const NamespaceString nss(parseNsOrUUID(opCtx, dbname, cmdObj)); //获取集合名或者UUID
        qr->refreshNSS(opCtx);

        // Fill out curop information.
        //
        // We pass negative values for 'ntoreturn' and 'ntoskip' to indicate that these values
        // should be omitted from the log line. Limit and skip information is already present in the
        // find command parameters, so these fields are redundant.
        const int ntoreturn = -1;
        const int ntoskip = -1;
        beginQueryOp(opCtx, nss, cmdObj, ntoreturn, ntoskip);

        // Finish the parsing step by using the QueryRequest to create a CanonicalQuery.
        ExtensionsCallbackReal extensionsCallback(opCtx, &nss);
        const boost::intrusive_ptr<ExpressionContext> expCtx;
        auto statusWithCQ =
			// Query会进行简单的处理(标准化)，并构造一些上下文数据结构变成CanonicalQuery(标准化Query)。
			//通过CanonicalQuery类的canonicalize函数进一步优化表达式树.
            CanonicalQuery::canonicalize(opCtx,
                                         std::move(qr),
                                         expCtx,
                                         extensionsCallback,
                                         MatchExpressionParser::kAllowAllSpecialFeatures &
                                             ~MatchExpressionParser::AllowedFeatures::kIsolated);
        if (!statusWithCQ.isOK()) {
            return appendCommandStatus(result, statusWithCQ.getStatus());
        }
        std::unique_ptr<CanonicalQuery> cq = std::move(statusWithCQ.getValue());

		//std::move(dbSLock)会调用DBLock::~DBLock
        AutoGetCollectionOrViewForReadCommand ctx(opCtx, nss, std::move(dbSLock));
        Collection* collection = ctx.getCollection();
        if (ctx.getView()) {
            // Relinquish locks. The aggregation command will re-acquire them.
            ctx.releaseLocksForView();

            // Convert the find command into an aggregation using $match (and other stages, as
            // necessary), if possible.
            const auto& qr = cq->getQueryRequest();
            auto viewAggregationCommand = qr.asAggregationCommand();
            if (!viewAggregationCommand.isOK())
                return appendCommandStatus(result, viewAggregationCommand.getStatus());

            BSONObj aggResult = Command::runCommandDirectly(
                opCtx,
                OpMsgRequest::fromDBAndBody(dbname, std::move(viewAggregationCommand.getValue())));
            auto status = getStatusFromCommandResult(aggResult);
            if (status.code() == ErrorCodes::InvalidPipelineOperator) {
                return appendCommandStatus(
                    result,
                    {ErrorCodes::InvalidPipelineOperator,
                     str::stream() << "Unsupported in view pipeline: " << status.reason()});
            }
            result.resetToEmpty();
            result.appendElements(aggResult);
            return status.isOK();
        }

        // Get the execution plan for the query.
        //StatusWith<unique_ptr<PlanExecutor, PlanExecutor::Deleter>> getExecutorFind,这里面会调用QueryPlanner::plan，获取对应的PlanExecutor
        ////根据CanonicalQuery得到的表达式树,调用getExecutor得到最终的PlanExecutor
        auto statusWithPlanExecutor = //获取std::unique_ptr<PlanExecutor, PlanExecutor::Deleter>
            getExecutorFind(opCtx, collection, nss, std::move(cq), PlanExecutor::YIELD_AUTO);
        if (!statusWithPlanExecutor.isOK()) {
            return appendCommandStatus(result, statusWithPlanExecutor.getStatus());
        }

        auto exec = std::move(statusWithPlanExecutor.getValue());

        {
            stdx::lock_guard<Client> lk(*opCtx->getClient());
            CurOp::get(opCtx)->setPlanSummary_inlock(Explain::getPlanSummary(exec.get()));
        }

        if (!collection) {
            // No collection. Just fill out curop indicating that there were zero results and
            // there is no ClientCursor id, and then return.
            const long long numResults = 0;
            const CursorId cursorId = 0;
            endQueryOp(opCtx, collection, *exec, numResults, cursorId);
            appendCursorResponseObject(cursorId, nss.ns(), BSONArray(), &result);
            return true;
        }

        const QueryRequest& originalQR = exec->getCanonicalQuery()->getQueryRequest();

        // Stream query results, adding them to a BSONArray as we go.
        CursorResponseBuilder firstBatch(/*isInitialResponse*/ true, &result);
        BSONObj obj;
        PlanExecutor::ExecState state = PlanExecutor::ADVANCED;
        long long numResults = 0;
		
		//获取obj信息，PlanExecutor::getNext->PlanExecutor::getNextImpl->PlanStage::work->FetchStage::doWork->PlanStage::work
		//->IndexScan::doWork->IndexScan::initIndexScan
        while (!FindCommon::enoughForFirstBatch(originalQR, numResults) &&
			   //获取满足条件的数据  //FindCmd::run循环调用PlanExecutor的getNext函数获得查询结果.
               PlanExecutor::ADVANCED == (state = exec->getNext(&obj, NULL))) {
            // If we can't fit this result inside the current batch, then we stash it for later.
            if (!FindCommon::haveSpaceForNext(obj, numResults, firstBatch.bytesUsed())) {
                exec->enqueue(obj); //PlanExecutor::enqueue  当取得的结果集满足一次返回的数量,将退出循环.
                break;
            }
/*
2018-11-01T15:50:35.766+0800 I QUERY	[conn1] yang test....FindCmd::run,OBJ:{ _id: 296446204, k: 2927256, c: "	 
57107967939-44137227834-24587740032-15980392750-15036476717-35100161171-04316050421-66523371550-06261438843-50432265427", 
pad: "	  13080577566-76793693218-00011035587-01443926745-80818518372", yangtest1: "	 13080577566-76793693218-00011035587
-01443926745-80818518372", yangtest2: " 	13080577566-76793693218-00011035587-01443926745-80818518372" }
*/
			log() << "yang test....FindCmd::run,OBJ:"<< (obj); //obj为按照PlanExecutor获取到的结果
            // Add result to output buffer.
            firstBatch.append(obj); //添加到firstBatch，在后面通过firstBatch.done返回客户端
            numResults++;
        }

        // Throw an assertion if query execution fails for any reason.
        if (PlanExecutor::FAILURE == state || PlanExecutor::DEAD == state) {
            firstBatch.abandon();
            error() << "Plan executor error during find command: " << PlanExecutor::statestr(state)
                    << ", stats: " << redact(Explain::getWinningPlanStats(exec.get()));

            return appendCommandStatus(result,
                                       Status(ErrorCodes::OperationFailed,
                                              str::stream()
                                                  << "Executor error during find command: "
                                                  << WorkingSetCommon::toStatusString(obj)));
        }

        // Before saving the cursor, ensure that whatever plan we established happened with the
        // expected collection version
        auto css = CollectionShardingState::get(opCtx, nss);
        css->checkShardVersionOrThrow(opCtx);

        // Set up the cursor for getMore.
        CursorId cursorId = 0;
		//如果有剩下的记录还没有取完,则保存游标cursorId,后续会调用getMore记录遍历游标.
        if (shouldSaveCursor(opCtx, collection, state, exec.get())) {
            // Create a ClientCursor containing this plan executor and register it with the cursor
            // manager.
            ClientCursorPin pinnedCursor = collection->getCursorManager()->registerCursor(
                opCtx,
                {std::move(exec),
                 nss,
                 AuthorizationSession::get(opCtx->getClient())->getAuthenticatedUserNames(),
                 opCtx->recoveryUnit()->isReadingFromMajorityCommittedSnapshot(),
                 cmdObj});
            cursorId = pinnedCursor.getCursor()->cursorid();

            invariant(!exec);
			//PlanExecutor按照执行计划一步一步迭代，获得最终的数据(或执行update修改数据)。
            PlanExecutor* cursorExec = pinnedCursor.getCursor()->getExecutor();

            // State will be restored on getMore.
            cursorExec->saveState();
            cursorExec->detachFromOperationContext();

            // We assume that cursors created through a DBDirectClient are always used from their
            // original OperationContext, so we do not need to move time to and from the cursor.
            if (!opCtx->getClient()->isInDirectClient()) {
                pinnedCursor.getCursor()->setLeftoverMaxTimeMicros(
                    opCtx->getRemainingMaxTimeMicros());
            }
            pinnedCursor.getCursor()->setPos(numResults);

            // Fill out curop based on the results.
            endQueryOp(opCtx, collection, *cursorExec, numResults, cursorId);
        } else {
            endQueryOp(opCtx, collection, *exec, numResults, cursorId);
        }

        // Generate the response object to send to the client.  返回结果集合和cursorId.
        firstBatch.done(cursorId, nss.ns());//CursorResponseBuilder::done 返回给客户端对应的数据
        return true;
    }

} findCmd;

}  // namespace
}  // namespace mongo
