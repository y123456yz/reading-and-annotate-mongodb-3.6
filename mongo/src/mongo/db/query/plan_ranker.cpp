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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kQuery

#include "mongo/platform/basic.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "mongo/db/query/plan_ranker.h"

#include "mongo/db/exec/plan_stage.h"
#include "mongo/db/exec/working_set.h"
#include "mongo/db/query/explain.h"
#include "mongo/db/query/query_knobs.h"
#include "mongo/db/query/query_solution.h"
#include "mongo/db/server_options.h"
#include "mongo/db/server_parameters.h"
#include "mongo/util/log.h"

namespace {

/**
 * Comparator for (scores, candidateIndex) in pickBestPlan().
 */
bool scoreComparator(const std::pair<double, size_t>& lhs, const std::pair<double, size_t>& rhs) {
    // Just compare score in lhs.first and rhs.first;
    // Ignore candidate array index in lhs.second and rhs.second.
    return lhs.first > rhs.first;
}

}  // namespace

namespace mongo {

using std::endl;
using std::vector;

//选择适合的索引  MultiPlanStage::pickBestPlan(PlanYieldPolicy* yieldPolicy)中调用PlanRanker::pickBestPlan(const vector<CandidatePlan>& candidates, PlanRankingDecision* why)
// static   MultiPlanStage::pickBestPlan中执行，配合MultiPlanStage::workAllPlans阅读
size_t PlanRanker::pickBestPlan(const vector<CandidatePlan>& candidates, PlanRankingDecision* why) {
    invariant(!candidates.empty());
    invariant(why);

    // A plan that hits EOF is automatically scored above
    // its peers. If multiple plans hit EOF during the same
    // set of round-robin calls to work(), then all such plans
    // receive the bonus.
    double eofBonus = 1.0;

    // Each plan will have a stat tree.
    std::vector<std::unique_ptr<PlanStageStats>> statTrees;

    // Get stat trees from each plan.
    // Copy stats trees instead of transferring ownership
    // because multi plan runner will need its own stats
    // trees for explain.
    for (size_t i = 0; i < candidates.size(); ++i) { //每个查询计划的分数记录到statTrees
        statTrees.push_back(candidates[i].root->getStats());
    }

    // Holds (score, candidateInndex).
    // Used to derive scores and candidate ordering.
    vector<std::pair<double, size_t>> scoresAndCandidateindices;

    // Compute score for each tree.  Record the best.
    for (size_t i = 0; i < statTrees.size(); ++i) {
        LOG(5) << "Scoring plan " << i << ":" << endl
               << redact(candidates[i].solution->toString()) << "Stats:\n"
               << redact(Explain::statsToBSON(*statTrees[i]).jsonString(Strict, true));
        LOG(2) << "Scoring query plan: " << redact(Explain::getPlanSummary(candidates[i].root))
               << " planHitEOF=" << statTrees[i]->common.isEOF;
		/*
		Mongodb是如何为查询选取认为合适的索引的呢？

		粗略来说，会先选几个候选的查询计划，然后会为这些查询计划按照某个规则来打分，分数最高的
		查询计划就是合适的查询计划，这个查询计划里面使用的索引就是认为合适的索引。
		*/
        double score = scoreTree(statTrees[i].get()); //打分
        LOG(5) << "score = " << score;
        if (statTrees[i]->common.isEOF) { //如果状态为IS_EOF则加一分，所以一般达到IS_EOF状态的索引都会被选中为最优执行计划。
            LOG(5) << "Adding +" << eofBonus << " EOF bonus to score.";
            score += 1;
        }

		//每个查询计划(对应i)及其分数score放入scoresAndCandidateindices数组对
        scoresAndCandidateindices.push_back(std::make_pair(score, i));
    }

    // Sort (scores, candidateIndex). Get best child and populate candidate ordering.
    //分数排序
    std::stable_sort(
        scoresAndCandidateindices.begin(), scoresAndCandidateindices.end(), scoreComparator);

    // Determine whether plans tied for the win.
    if (scoresAndCandidateindices.size() > 1U) {
        double bestScore = scoresAndCandidateindices[0].first;
        double runnerUpScore = scoresAndCandidateindices[1].first;
        const double epsilon = 1e-10;
		//最优的查询计划比第二优的查询计划得分小于1e-10，tieForBest为1，否则为0，
        why->tieForBest = std::abs(bestScore - runnerUpScore) < epsilon;
    }

    // Update results in 'why'
    // Stats and scores in 'why' are sorted in descending order by score.
    why->stats.clear();
    why->scores.clear();
    why->candidateOrder.clear();
    for (size_t i = 0; i < scoresAndCandidateindices.size(); ++i) {
        double score = scoresAndCandidateindices[i].first;
        size_t candidateIndex = scoresAndCandidateindices[i].second;

        // We shouldn't cache the scores with the EOF bonus included,
        // as this is just a tie-breaking measure for plan selection.
        // Plans not run through the multi plan runner will not receive
        // the bonus.
        //
        // An example of a bad thing that could happen if we stored scores
        // with the EOF bonus included:
        //
        //   Let's say Plan A hits EOF, is the highest ranking plan, and gets
        //   cached as such. On subsequent runs it will not receive the bonus.
        //   Eventually the plan cache feedback mechanism will evict the cache
        //   entry---the scores will appear to have fallen due to the missing
        //   EOF bonus.
        //
        // This begs the question, why don't we include the EOF bonus in
        // scoring of cached plans as well? The problem here is that the cached
        // plan runner always runs plans to completion before scoring. Queries
        // that don't get the bonus in the multi plan runner might get the bonus
        // after being run from the plan cache.
        if (statTrees[candidateIndex]->common.isEOF) {
            score -= eofBonus;
        }

        why->stats.push_back(std::move(statTrees[candidateIndex]));
        why->scores.push_back(score);
        why->candidateOrder.push_back(candidateIndex);
    }

    size_t bestChild = scoresAndCandidateindices[0].second; //第0个成员是分数最高的，也就是最优的
    return bestChild;
}

// TODO: Move this out.  This is a signal for ranking but will become its own complicated
// stats-collecting beast.
double computeSelectivity(const PlanStageStats* stats) {
    if (STAGE_IXSCAN == stats->stageType) {
        IndexScanStats* iss = static_cast<IndexScanStats*>(stats->specific.get());
        return iss->keyPattern.nFields();
    } else {
        double sum = 0;
        for (size_t i = 0; i < stats->children.size(); ++i) {
            sum += computeSelectivity(stats->children[i].get());
        }
        return sum;
    }
}

bool hasStage(const StageType type, const PlanStageStats* stats) {
    if (type == stats->stageType) {
        return true;
    }
    for (size_t i = 0; i < stats->children.size(); ++i) {
        if (hasStage(type, stats->children[i].get())) {
            return true;
        }
    }
    return false;
}

/*
Mongodb是如何为查询选取认为合适的索引的呢？

粗略来说，会先选几个候选的查询计划，然后会为这些查询计划按照某个规则来打分，分数最高的
查询计划就是合适的查询计划，这个查询计划里面使用的索引就是认为合适的索引。
*/ //scoreTree里面就是计算每个查询计划的得分
/*
scoreTree并没有执行查询，只是根据已有的PlanStageStats* stats来进行计算。那么，
是什么时候执行查询来获取查询计划的PlanStageStats* stats的呢？

在mongo::MultiPlanStage::pickBestPlan（代码位于src/mongo/db/exec/multi_plan.cpp）中，会
调用workAllPlans来执行所有的查询计划，最多会调用numWorks次
*/ //PlanRanker::pickBestPlan(const vector<CandidatePlan>& candidates, PlanRankingDecision* why)中调用
// static    stats在workAllPlans中获取stats
double PlanRanker::scoreTree(const PlanStageStats* stats) {
    // We start all scores at 1.  Our "no plan selected" score is 0 and we want all plans to
    // be greater than that.
    double baseScore = 1;

    // How many "units of work" did the plan perform. Each call to work(...)
    // counts as one unit.
    size_t workUnits = stats->common.works;
    invariant(workUnits != 0);

    // How much did a plan produce?
    // Range: [0, 1]
    //这里的common.advanced是每个索引扫描的时候是否能在collection拿到符合条件的记录，
    //如果能拿到记录那么common.advanced就加1，workUnits则是总共扫描的次数
    double productivity =
        static_cast<double>(stats->common.advanced) / static_cast<double>(workUnits);

    // Just enough to break a tie. Must be small enough to ensure that a more productive
    // plan doesn't lose to a less productive plan due to tie breaking.
    const double epsilon = std::min(1.0 / static_cast<double>(10 * workUnits), 1e-4);

    // We prefer covered projections.
    //
    // We only do this when we have a projection stage because we have so many jstests that
    // check bounds even when a collscan plan is just as good as the ixscan'd plan :(
    double noFetchBonus = epsilon;
	//STAGE_PROJECTION&&STAGE_FETCH（限定返回字段）
    if (hasStage(STAGE_PROJECTION, stats) && hasStage(STAGE_FETCH, stats)) {
        noFetchBonus = 0;
    }

    // In the case of ties, prefer solutions without a blocking sort
    // to solutions with a blocking sort.
    double noSortBonus = epsilon;
    if (hasStage(STAGE_SORT, stats)) { //STAGE_SORT（避免排序）
        noSortBonus = 0;
    }

    // In the case of ties, prefer single index solutions to ixisect. Index
    // intersection solutions are often slower than single-index solutions
    // because they require examining a superset of index keys that would be
    // examined by a single index scan.
    //
    // On the other hand, index intersection solutions examine the same
    // number or fewer of documents. In the case that index intersection
    // allows us to examine fewer documents, the penalty given to ixisect
    // can be made up via the no fetch bonus.
    double noIxisectBonus = epsilon;
	//STAGE_AND_HASH || STAGE_AND_SORTED（这个主要在交叉索引时产生）
    if (hasStage(STAGE_AND_HASH, stats) || hasStage(STAGE_AND_SORTED, stats)) {
        noIxisectBonus = 0;
    }

	/*
	然后我们再看tieBreakers，它是由noFetchBonus和noSortBonus和noIxisectBonus总和构成的。我们根据上述代码
	可以看到这三个值主要是控制MongoDB不要选择如下状态的：
	
	STAGE_PROJECTION&&STAGE_FETCH（限定返回字段）
	STAGE_SORT（避免排序）
	STAGE_AND_HASH || STAGE_AND_SORTED（这个主要在交叉索引时产生）
	它们的出现都比较影响性能，所以一旦它们出现，相应的值都会被设置成0.
	*/
    double tieBreakers = noFetchBonus + noSortBonus + noIxisectBonus;
    double score = baseScore + productivity + tieBreakers;

    mongoutils::str::stream ss;
    ss << "score(" << score << ") = baseScore(" << baseScore << ")"
    //LOG(2) << "score(" << score << ") = baseScore(" << baseScore << ")"
       << " + productivity((" << stats->common.advanced << " advanced)/(" << stats->common.works
       << " works) = " << productivity << ")"
       << " + tieBreakers(" << noFetchBonus << " noFetchBonus + " << noSortBonus
       << " noSortBonus + " << noIxisectBonus << " noIxisectBonus = " << tieBreakers << ")";
    std::string scoreStr = ss;
    LOG(2) << scoreStr;

    if (internalQueryForceIntersectionPlans.load()) {
        if (hasStage(STAGE_AND_HASH, stats) || hasStage(STAGE_AND_SORTED, stats)) {
            // The boost should be >2.001 to make absolutely sure the ixisect plan will win due
            // to the combination of 1) productivity, 2) eof bonus, and 3) no ixisect bonus.
            score += 3;
            LOG(5) << "Score boosted to " << score << " due to intersection forcing.";
        }
    }

    return score;
}

}  // namespace mongo
