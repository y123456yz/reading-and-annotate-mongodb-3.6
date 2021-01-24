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

#include "mongo/db/exec/multi_plan.h"

#include <algorithm>
#include <math.h>

#include "mongo/base/owned_pointer_vector.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/database.h"
#include "mongo/db/client.h"
#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/exec/scoped_timer.h"
#include "mongo/db/exec/working_set_common.h"
#include "mongo/db/query/explain.h"
#include "mongo/db/query/plan_cache.h"
#include "mongo/db/query/plan_ranker.h"
#include "mongo/db/storage/record_fetcher.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

using std::endl;
using std::list;
using std::unique_ptr;
using std::vector;
using stdx::make_unique;

// static
const char* MultiPlanStage::kStageType = "MULTI_PLAN";

MultiPlanStage::MultiPlanStage(OperationContext* opCtx,
                               const Collection* collection,
                               CanonicalQuery* cq,
                               CachingMode cachingMode)
    : PlanStage(kStageType, opCtx),
      _collection(collection),
      _cachingMode(cachingMode),
      _query(cq),
      _bestPlanIdx(kNoSuchPlan),
      _backupPlanIdx(kNoSuchPlan),
      _failure(false),
      _failureCount(0),
      _statusMemberId(WorkingSet::INVALID_ID) {
    invariant(_collection);
}

//prepareExecution中执行
void MultiPlanStage::addPlan(QuerySolution* solution, PlanStage* root, WorkingSet* ws) {
    _candidates.push_back(CandidatePlan(solution, root, ws));
    _children.emplace_back(root); //PlanStage._children
}

bool MultiPlanStage::isEOF() {
    if (_failure) {
        return true;
    }

    // If _bestPlanIdx hasn't been found, can't be at EOF
    if (!bestPlanChosen()) {
        return false;
    }

    // We must have returned all our cached results
    // and there must be no more results from the best plan.
    CandidatePlan& bestPlan = _candidates[_bestPlanIdx];
    return bestPlan.results.empty() && bestPlan.root->isEOF();
}

/*
(gdb) bt
#0  mongo::MultiPlanStage::doWork (this=0x7fd018909200, out=0x7fd0112938d0) at src/mongo/db/exec/multi_plan.cpp:111
#1  0x00007fd01255d625 in mongo::PlanStage::work (this=0x7fd018909200, out=out@entry=0x7fd0112938d0) at src/mongo/db/exec/plan_stage.cpp:88
#2  0x00007fd01223291a in mongo::PlanExecutor::getNextImpl (this=0x7fd018909400, objOut=objOut@entry=0x7fd0112939d0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:621
#3  0x00007fd01223342b in mongo::PlanExecutor::getNext (this=<optimized out>, objOut=objOut@entry=0x7fd011293af0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:449
#4  0x00007fd011e97b95 in mongo::(anonymous namespace)::FindCmd::run
*/ 
//如果有多个索引满足条件，则选择最优的执行
PlanStage::StageState MultiPlanStage::doWork(WorkingSetID* out) {
    if (_failure) {
        *out = _statusMemberId;
        return PlanStage::FAILURE;
    }
	LOG(2) << "yang test ...PlanStage::StageState MultiPlanStage::doWork ";

	//选择这个最优的查询计划
    CandidatePlan& bestPlan = _candidates[_bestPlanIdx];

    // Look for an already produced result that provides the data the caller wants.
    if (!bestPlan.results.empty()) {
        *out = bestPlan.results.front();
        bestPlan.results.pop_front();
        return PlanStage::ADVANCED;
    }

    // best plan had no (or has no more) cached results

    StageState state = bestPlan.root->work(out);  //执行最优的stage

    if (PlanStage::FAILURE == state && hasBackupPlan()) {
        LOG(2) << "Best plan errored out switching to backup";
        // Uncache the bad solution if we fall back
        // on the backup solution.
        //
        // XXX: Instead of uncaching we should find a way for the
        // cached plan runner to fall back on a different solution
        // if the best solution fails. Alternatively we could try to
        // defer cache insertion to be after the first produced result.

        _collection->infoCache()->getPlanCache()->remove(*_query).transitional_ignore();

        _bestPlanIdx = _backupPlanIdx;
        _backupPlanIdx = kNoSuchPlan;

        return _candidates[_bestPlanIdx].root->work(out);
    }

    if (hasBackupPlan() && PlanStage::ADVANCED == state) {
        LOG(2) << "Best plan had a blocking stage, became unblocked";
        _backupPlanIdx = kNoSuchPlan;
    }

    return state;
}

//检查操作是否被kill，没有则让出CPU资源
Status MultiPlanStage::tryYield(PlanYieldPolicy* yieldPolicy) {
    // These are the conditions which can cause us to yield:
    //   1) The yield policy's timer elapsed, or
    //   2) some stage requested a yield due to a document fetch, or
    //   3) we need to yield and retry due to a WriteConflictException.
    // In all cases, the actual yielding happens here.
    if (yieldPolicy->shouldYield()) {
        auto yieldStatus = yieldPolicy->yield(_fetcher.get());

        if (!yieldStatus.isOK()) {
            _failure = true;
            _statusMemberId =
                WorkingSetCommon::allocateStatusMember(_candidates[0].ws, yieldStatus);
            return yieldStatus;
        }
    }

    // We're done using the fetcher, so it should be freed. We don't want to
    // use the same RecordFetcher twice.
    _fetcher.reset();

    return Status::OK();
}

//参考http://mongoing.com/archives/5624?spm=a2c4e.11153940.blogcont647563.13.6ee0730cDKb7RN
//MultiPlanStage::pickBestPlan中执行
//获取采样数，collection的总记录数*0.29如果比10000小就扫描10000次，如果比10000大那么就扫描collection数量*0.29次。  
// static     
size_t MultiPlanStage::getTrialPeriodWorks(OperationContext* opCtx, const Collection* collection) {
    // Run each plan some number of times. This number is at least as great as
    // 'internalQueryPlanEvaluationWorks', but may be larger for big collections.
    size_t numWorks = internalQueryPlanEvaluationWorks.load();
    if (NULL != collection) {
		/*
		internalQueryPlanEvaluationWorks=10000
		fraction=0.29
		collection-&gt;numRecords(txn) 则为collection的总记录数
		那就是collection的总记录数*0.29如果比10000小就扫描10000次，如果比10000大那么就扫描collection数量*0.29次。
		*/
        // For large collections, the number of works is set to be this
        // fraction of the collection size.
        double fraction = internalQueryPlanEvaluationCollFraction;

        numWorks = std::max(static_cast<size_t>(internalQueryPlanEvaluationWorks.load()),
							// WiredTigerRecordStore::numRecords   collection的总记录数
                            static_cast<size_t>(fraction * collection->numRecords(opCtx)));
    }

    return numWorks;
}

//获取请求中NToReturn  limit 和internalQueryPlanEvaluationMaxResults的最小值
// static
size_t MultiPlanStage::getTrialPeriodNumToReturn(const CanonicalQuery& query) {
    // Determine the number of results which we will produce during the plan
    // ranking phase before stopping.
    size_t numResults = static_cast<size_t>(internalQueryPlanEvaluationMaxResults.load());
    if (query.getQueryRequest().getNToReturn()) {
        numResults =
            std::min(static_cast<size_t>(*query.getQueryRequest().getNToReturn()), numResults);
    } else if (query.getQueryRequest().getLimit()) {
        numResults = std::min(static_cast<size_t>(*query.getQueryRequest().getLimit()), numResults);
    }

    return numResults;
}

//MultiPlanStage::pickBestPlan(PlanYieldPolicy* yieldPolicy)中调用PlanRanker::pickBestPlan(const vector<CandidatePlan>& candidates, PlanRankingDecision* why)
/*
lldb调用栈
mongo::PlanRanker::scoreTree
mongo::PlanRanker::pickBestPlan
mongo::MultiPlanStage::pickBestPlan
mongo::PlanExecutor::pickBestPlan
mongo::PlanExecutor::make
mongo::PlanExecutor::make
mongo::getExecutor
mongo::getExecutorFind
mongo::FindCmd::explain
*/
/*
Mongodb是如何为查询选取认为合适的索引的呢？

粗略来说，会先选几个候选的查询计划，然后会为这些查询计划按照某个规则来打分，分数最高
的查询计划就是合适的查询计划，这个查询计划里面使用的索引就是认为合适的索引。
*/ //https://yq.aliyun.com/articles/74635
//从PlanExecutor::pickBestPlan调用该函数
Status MultiPlanStage::pickBestPlan(PlanYieldPolicy* yieldPolicy) {
    // Adds the amount of time taken by pickBestPlan() to executionTimeMillis. There's lots of
    // execution work that happens here, so this is needed for the time accounting to
    // make sense.
    ScopedTimer timer(getClock(), &_commonStats.executionTimeMillis); 

	//获取采样数，collection的总记录数*0.29如果比10000小就扫描10000次，如果比10000大那么就扫描collection数量*0.29次。  
    size_t numWorks = getTrialPeriodWorks(getOpCtx(), _collection);
	//获取请求中NToReturn  limit 和internalQueryPlanEvaluationMaxResults的最小值
    size_t numResults = getTrialPeriodNumToReturn(*_query);

    // Work the plans, stopping when a plan hits EOF or returns some
    // fixed number of results.
    for (size_t ix = 0; ix < numWorks; ++ix) {
		//workAllPlans执行所有的查询计划，MultiPlanStage::pickBestPlan最多会调用numWorks次
        bool moreToDo = workAllPlans(numResults, yieldPolicy);
		//只要候选_candidates中的任何一个获取到的数据到了numResults或者达到IS_EOF，就退出这个for循环
        if (!moreToDo) {
            break;
        }
    }

    if (_failure) {
        invariant(WorkingSet::INVALID_ID != _statusMemberId);
        WorkingSetMember* member = _candidates[0].ws->get(_statusMemberId);
        return WorkingSetCommon::getMemberStatus(*member);
    }

    // After picking best plan, ranking will own plan stats from
    // candidate solutions (winner and losers).
    std::unique_ptr<PlanRankingDecision> ranking(new PlanRankingDecision);

	
	//MultiPlanStage::pickBestPlan(PlanYieldPolicy* yieldPolicy)中调用
	//PlanRanker::pickBestPlan(const vector<CandidatePlan>& candidates, PlanRankingDecision* why)
	//MultiPlanStage::doWork中获取真正数据的时候用到
    _bestPlanIdx = PlanRanker::pickBestPlan(_candidates, ranking.get()); //选择最优的查询计划
    verify(_bestPlanIdx >= 0 && _bestPlanIdx < static_cast<int>(_candidates.size()));

    // Copy candidate order. We will need this to sort candidate stats for explain
    // after transferring ownership of 'ranking' to plan cache.
    std::vector<size_t> candidateOrder = ranking->candidateOrder;

	//最优的查询计划最终在MultiPlanStage::doWork中执行
    CandidatePlan& bestCandidate = _candidates[_bestPlanIdx];
    std::list<WorkingSetID>& alreadyProduced = bestCandidate.results;
    const auto& bestSolution = bestCandidate.solution;

    LOG(2) << "Winning solution:\n" << redact(bestSolution->toString());
    LOG(2) << "Winning plan: " << redact(Explain::getPlanSummary(bestCandidate.root));

    _backupPlanIdx = kNoSuchPlan;
	//如果得分第一高的候选计划有阻塞的可能，则选择第二得分高的候选计划，以此类推
    if (bestSolution->hasBlockingStage && (0 == alreadyProduced.size())) { //该查询计划有阻塞情况，则选择备用的
        LOG(2) << "Winner has blocking stage, looking for backup plan...";
        for (size_t ix = 0; ix < _candidates.size(); ++ix) {
            if (!_candidates[ix].solution->hasBlockingStage) {
                LOG(2) << "Candidate " << ix << " is backup child";
                _backupPlanIdx = ix;
                break;
            }
        }
    }

    // Even if the query is of a cacheable shape, the caller might have indicated that we shouldn't
    // write to the plan cache.
    //
    // TODO: We can remove this if we introduce replanning logic to the SubplanStage.
    //_cachingMode默认赋值为CachingMode::AlwaysCache
    bool canCache = (_cachingMode == CachingMode::AlwaysCache);
    if (_cachingMode == CachingMode::SometimesCache) { //一般不走该流程
        // In "sometimes cache" mode, we cache unless we hit one of the special cases below.
        canCache = true;

        if (ranking->tieForBest) {
            // The winning plan tied with the runner-up and we're using "sometimes cache" mode. We
            // will not write a plan cache entry.
            canCache = false;

            // These arrays having two or more entries is implied by 'tieForBest'.
            invariant(ranking->scores.size() > 1U);
            invariant(ranking->candidateOrder.size() > 1U);

            size_t winnerIdx = ranking->candidateOrder[0];
            size_t runnerUpIdx = ranking->candidateOrder[1];

            LOG(1) << "Winning plan tied with runner-up. Not caching."
                   << " ns: " << _collection->ns() << " " << redact(_query->toStringShort())
                   << " winner score: " << ranking->scores[0] << " winner summary: "
                   << redact(Explain::getPlanSummary(_candidates[winnerIdx].root))
                   << " runner-up score: " << ranking->scores[1] << " runner-up summary: "
                   << redact(Explain::getPlanSummary(_candidates[runnerUpIdx].root));
        }

        if (alreadyProduced.empty()) {
            // We're using the "sometimes cache" mode, and the winning plan produced no results
            // during the plan ranking trial period. We will not write a plan cache entry.
            canCache = false;

            size_t winnerIdx = ranking->candidateOrder[0];
            LOG(1) << "Winning plan had zero results. Not caching."
                   << " ns: " << _collection->ns() << " " << redact(_query->toStringShort())
                   << " winner score: " << ranking->scores[0] << " winner summary: "
                   << redact(Explain::getPlanSummary(_candidates[winnerIdx].root));
        }
    }

    // Store the choice we just made in the cache, if the query is of a type that is safe to
    // cache.
    if (PlanCache::shouldCacheQuery(*_query) && canCache) { //把最优的前面几个QuerySolution缓存起来
        // Create list of candidate solutions for the cache with
        // the best solution at the front.
        std::vector<QuerySolution*> solutions;

        // Generate solutions and ranking decisions sorted by score.
        //所有查询计划及其得分保存到solutions数组
        for (size_t orderingIndex = 0; orderingIndex < candidateOrder.size(); ++orderingIndex) {
            // index into candidates/ranking
            size_t ix = candidateOrder[orderingIndex];
            solutions.push_back(_candidates[ix].solution.get());
        }

        // Check solution cache data. Do not add to cache if
        // we have any invalid SolutionCacheData data.
        // XXX: One known example is 2D queries
        bool validSolutions = true;
        for (size_t ix = 0; ix < solutions.size(); ++ix) {
            if (NULL == solutions[ix]->cacheData.get()) {
                LOG(2) << "Not caching query because this solution has no cache data: "
                       << redact(solutions[ix]->toString());
                validSolutions = false;
                break;
            }
        }

        if (validSolutions) { //把这些solutions添加到plancache，下次就可以直接用plancache来执行
            _collection->infoCache()
                ->getPlanCache()
                ->add(*_query,
                      solutions,
                      //得分排序好的候选查询计划放入到plancache中缓存
                      ranking.release(),
                      getOpCtx()->getServiceContext()->getPreciseClockSource()->now())
                .transitional_ignore();
        }
    }

    return Status::OK();
}

//MultiPlanStage::pickBestPlan   https://segmentfault.com/a/1190000015236644  https://yq.aliyun.com/articles/74635
//workAllPlans执行所有的查询计划，MultiPlanStage::pickBestPlan最多会调用numWorks次，PlanRanker::pickBestPlan中根据这里的work选择最优的
bool MultiPlanStage::workAllPlans(size_t numResults, PlanYieldPolicy* yieldPolicy) {
    bool doneWorking = false;

	//候选的查询计划存放在_candidates数组中的
    for (size_t ix = 0; ix < _candidates.size(); ++ix) {
        CandidatePlan& candidate = _candidates[ix];
        if (candidate.failed) {
            continue;
        }

        // Might need to yield between calls to work due to the timer elapsing.
        //检查操作是否被kill，没有则让出CPU资源
        if (!(tryYield(yieldPolicy)).isOK()) {
            return false;
        }

        WorkingSetID id = WorkingSet::INVALID_ID;
		//执行对应PlanStage::work， 不同类型PlanStage可以参考buildStages，
		//包括CollectionScan、IndexScan等，CollectionScan::work  IndexScan::work
        PlanStage::StageState state = candidate.root->work(&id); //PlanStage::work

		//
        if (PlanStage::ADVANCED == state) {
            // Save result for later.
            //获取对应的结果，例如CollectionScan::work、IndexScan::work分别代表获取到doc数据、索引数据
            WorkingSetMember* member = candidate.ws->get(id);
            // Ensure that the BSONObj underlying the WorkingSetMember is owned in case we choose to
            // return the results from the 'candidate' plan.
            member->makeObjOwnedIfNeeded();
			//每次调用PlanStage::work的结果id都存储到candidate.results
            candidate.results.push_back(id);

            // Once a plan returns enough results, stop working.
            if (candidate.results.size() >= numResults) {
				//该candidate候选plan已经获取到了足够的results
                doneWorking = true;
            }
        } else if (PlanStage::IS_EOF == state) {
            // First plan to hit EOF wins automatically.  Stop evaluating other plans.
            // Assumes that the ranking will pick this plan.
            doneWorking = true;
        } else if (PlanStage::NEED_YIELD == state) {
            if (id == WorkingSet::INVALID_ID) {
                if (!yieldPolicy->canAutoYield())
                    throw WriteConflictException();
            } else {
                WorkingSetMember* member = candidate.ws->get(id);
                invariant(member->hasFetcher());
                // Transfer ownership of the fetcher and yield.
                _fetcher.reset(member->releaseFetcher());
            }

            if (yieldPolicy->canAutoYield()) {
                yieldPolicy->forceYield();
            }

            if (!(tryYield(yieldPolicy)).isOK()) {
                return false;
            }
        } else if (PlanStage::NEED_TIME != state) {
            // FAILURE or DEAD.  Do we want to just tank that plan and try the rest?  We
            // probably want to fail globally as this shouldn't happen anyway.

            candidate.failed = true;
            ++_failureCount;

            // Propagate most recent seen failure to parent.
            if (PlanStage::FAILURE == state) {
                _statusMemberId = id;
            }

            if (_failureCount == _candidates.size()) {
                _failure = true;
                return false;
            }
        }
    }

    return !doneWorking;
}

namespace {

void invalidateHelper(OperationContext* opCtx,
                      WorkingSet* ws,  // may flag for review
                      const RecordId& recordId,
                      list<WorkingSetID>* idsToInvalidate,
                      const Collection* collection) {
    for (auto it = idsToInvalidate->begin(); it != idsToInvalidate->end(); ++it) {
        WorkingSetMember* member = ws->get(*it);
        if (member->hasRecordId() && member->recordId == recordId) {
            WorkingSetCommon::fetchAndInvalidateRecordId(opCtx, member, collection);
        }
    }
}

}  // namespace

void MultiPlanStage::doInvalidate(OperationContext* opCtx,
                                  const RecordId& recordId,
                                  InvalidationType type) {
    if (_failure) {
        return;
    }

    if (bestPlanChosen()) {
        CandidatePlan& bestPlan = _candidates[_bestPlanIdx];
        invalidateHelper(opCtx, bestPlan.ws, recordId, &bestPlan.results, _collection);
        if (hasBackupPlan()) {
            CandidatePlan& backupPlan = _candidates[_backupPlanIdx];
            invalidateHelper(opCtx, backupPlan.ws, recordId, &backupPlan.results, _collection);
        }
    } else {
        for (size_t ix = 0; ix < _candidates.size(); ++ix) {
            invalidateHelper(
                opCtx, _candidates[ix].ws, recordId, &_candidates[ix].results, _collection);
        }
    }
}

bool MultiPlanStage::hasBackupPlan() const {
    return kNoSuchPlan != _backupPlanIdx;
}

bool MultiPlanStage::bestPlanChosen() const {
    return kNoSuchPlan != _bestPlanIdx;
}

int MultiPlanStage::bestPlanIdx() const {
    return _bestPlanIdx;
}

QuerySolution* MultiPlanStage::bestSolution() {
    if (_bestPlanIdx == kNoSuchPlan)
        return NULL;

    return _candidates[_bestPlanIdx].solution.get();
}

unique_ptr<PlanStageStats> MultiPlanStage::getStats() {
    _commonStats.isEOF = isEOF();
	//MultiPlanStage对应统计 
    unique_ptr<PlanStageStats> ret = make_unique<PlanStageStats>(_commonStats, STAGE_MULTI_PLAN);
    ret->specific = make_unique<MultiPlanStats>(_specificStats);
    for (auto&& child : _children) {
        ret->children.emplace_back(child->getStats());
    }
    return ret;
}

const SpecificStats* MultiPlanStage::getSpecificStats() const {
    return &_specificStats;
}

}  // namespace mongo

