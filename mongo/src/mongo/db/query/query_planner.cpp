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

#include "mongo/db/query/query_planner.h"

#include <boost/optional.hpp>
#include <vector>

#include "mongo/base/string_data.h"
#include "mongo/bson/simple_bsonelement_comparator.h"
#include "mongo/client/dbclientinterface.h"  // For QueryOption_foobar
#include "mongo/db/bson/dotted_path_support.h"
#include "mongo/db/matcher/expression_algo.h"
#include "mongo/db/matcher/expression_geo.h"
#include "mongo/db/matcher/expression_text.h"
#include "mongo/db/query/canonical_query.h"
#include "mongo/db/query/collation/collation_index_key.h"
#include "mongo/db/query/collation/collator_interface.h"
#include "mongo/db/query/plan_cache.h"
#include "mongo/db/query/plan_enumerator.h"
#include "mongo/db/query/planner_access.h"
#include "mongo/db/query/planner_analysis.h"
#include "mongo/db/query/planner_ixselect.h"
#include "mongo/db/query/query_planner_common.h"
#include "mongo/db/query/query_solution.h"
#include "mongo/util/log.h"

namespace mongo {

using std::unique_ptr;
using std::numeric_limits;

namespace dps = ::mongo::dotted_path_support;

// Copied verbatim from db/index.h
static bool isIdIndex(const BSONObj& pattern) {
    BSONObjIterator i(pattern);
    BSONElement e = i.next();
    //_id index must have form exactly {_id : 1} or {_id : -1}.
    // Allows an index of form {_id : "hashed"} to exist but
    // do not consider it to be the primary _id index
    if (!(strcmp(e.fieldName(), "_id") == 0 && (e.numberInt() == 1 || e.numberInt() == -1)))
        return false;
    return i.next().eoo();
}

static bool is2DIndex(const BSONObj& pattern) {
    BSONObjIterator it(pattern);
    while (it.more()) {
        BSONElement e = it.next();
        if (String == e.type() && str::equals("2d", e.valuestr())) {
            return true;
        }
    }
    return false;
}

string optionString(size_t options) {
    mongoutils::str::stream ss;

    if (QueryPlannerParams::DEFAULT == options) {
        ss << "DEFAULT ";
    }
    while (options) {
        // The expression (x & (x - 1)) yields x with the lowest bit cleared.  Then the exclusive-or
        // of the result with the original yields the lowest bit by itself.
        size_t new_options = options & (options - 1);
        QueryPlannerParams::Options opt = QueryPlannerParams::Options(new_options ^ options);
        options = new_options;
        switch (opt) {
            case QueryPlannerParams::NO_TABLE_SCAN:
                ss << "NO_TABLE_SCAN ";
                break;
            case QueryPlannerParams::INCLUDE_COLLSCAN:
                ss << "INCLUDE_COLLSCAN ";
                break;
            case QueryPlannerParams::INCLUDE_SHARD_FILTER:
                ss << "INCLUDE_SHARD_FILTER ";
                break;
            case QueryPlannerParams::NO_BLOCKING_SORT:
                ss << "NO_BLOCKING_SORT ";
                break;
            case QueryPlannerParams::INDEX_INTERSECTION:
                ss << "INDEX_INTERSECTION ";
                break;
            case QueryPlannerParams::KEEP_MUTATIONS:
                ss << "KEEP_MUTATIONS ";
                break;
            case QueryPlannerParams::IS_COUNT:
                ss << "IS_COUNT ";
                break;
            case QueryPlannerParams::SPLIT_LIMITED_SORT:
                ss << "SPLIT_LIMITED_SORT ";
                break;
            case QueryPlannerParams::CANNOT_TRIM_IXISECT:
                ss << "CANNOT_TRIM_IXISECT ";
                break;
            case QueryPlannerParams::SNAPSHOT_USE_ID:
                ss << "SNAPSHOT_USE_ID ";
                break;
            case QueryPlannerParams::NO_UNCOVERED_PROJECTIONS:
                ss << "NO_UNCOVERED_PROJECTIONS ";
                break;
            case QueryPlannerParams::GENERATE_COVERED_IXSCANS:
                ss << "GENERATE_COVERED_IXSCANS ";
                break;
            case QueryPlannerParams::TRACK_LATEST_OPLOG_TS:
                ss << "TRACK_LATEST_OPLOG_TS ";
            case QueryPlannerParams::DEFAULT:
                MONGO_UNREACHABLE;
                break;
        }
    }

    return ss;
}

static BSONObj getKeyFromQuery(const BSONObj& keyPattern, const BSONObj& query) {
    return query.extractFieldsUnDotted(keyPattern);
}

static bool indexCompatibleMaxMin(const BSONObj& obj,
                                  const CollatorInterface* queryCollator,
                                  const IndexEntry& indexEntry) {
    BSONObjIterator kpIt(indexEntry.keyPattern);
    BSONObjIterator objIt(obj);

    const bool collatorsMatch =
        CollatorInterface::collatorsMatch(queryCollator, indexEntry.collator);

    for (;;) {
        // Every element up to this point has matched so the KP matches
        if (!kpIt.more() && !objIt.more()) {
            return true;
        }

        // If only one iterator is done, it's not a match.
        if (!kpIt.more() || !objIt.more()) {
            return false;
        }

        // Field names must match and be in the same order.
        BSONElement kpElt = kpIt.next();
        BSONElement objElt = objIt.next();
        if (!mongoutils::str::equals(kpElt.fieldName(), objElt.fieldName())) {
            return false;
        }

        // If the index collation doesn't match the query collation, and the min/max obj has a
        // boundary value that needs to respect the collation, then the index is not compatible.
        if (!collatorsMatch && CollationIndexKey::isCollatableType(objElt.type())) {
            return false;
        }
    }
}

static BSONObj stripFieldNamesAndApplyCollation(const BSONObj& obj,
                                                const CollatorInterface* collator) {
    BSONObjBuilder bob;
    for (BSONElement elt : obj) {
        CollationIndexKey::collationAwareIndexKeyAppend(elt, collator, &bob);
    }
    return bob.obj();
}

/**
 * "Finishes" the min object for the $min query option by filling in an empty object with
 * MinKey/MaxKey and stripping field names. Also translates keys according to the collation, if
 * necessary.
 *
 * In the case that 'minObj' is empty, we "finish" it by filling in either MinKey or MaxKey
 * instead. Choosing whether to use MinKey or MaxKey is done by comparing against 'maxObj'.
 * For instance, suppose 'minObj' is empty, 'maxObj' is { a: 3 }, and the key pattern is
 * { a: -1 }. According to the key pattern ordering, { a: 3 } < MinKey. This means that the
 * proper resulting bounds are
 *
 *   start: { '': MaxKey }, end: { '': 3 }
 *
 * as opposed to
 *
 *   start: { '': MinKey }, end: { '': 3 }
 *
 * Suppose instead that the key pattern is { a: 1 }, with the same 'minObj' and 'maxObj'
 * (that is, an empty object and { a: 3 } respectively). In this case, { a: 3 } > MinKey,
 * which means that we use range [{'': MinKey}, {'': 3}]. The proper 'minObj' in this case is
 * MinKey, whereas in the previous example it was MaxKey.
 *
 * If 'minObj' is non-empty, then all we do is strip its field names (because index keys always
 * have empty field names).
 */
static BSONObj finishMinObj(const IndexEntry& indexEntry,
                            const BSONObj& minObj,
                            const BSONObj& maxObj) {
    BSONObjBuilder bob;
    bob.appendMinKey("");
    BSONObj minKey = bob.obj();

    if (minObj.isEmpty()) {
        if (0 > minKey.woCompare(maxObj, indexEntry.keyPattern, false)) {
            BSONObjBuilder minKeyBuilder;
            minKeyBuilder.appendMinKey("");
            return minKeyBuilder.obj();
        } else {
            BSONObjBuilder maxKeyBuilder;
            maxKeyBuilder.appendMaxKey("");
            return maxKeyBuilder.obj();
        }
    } else {
        return stripFieldNamesAndApplyCollation(minObj, indexEntry.collator);
    }
}

/**
 * "Finishes" the max object for the $max query option by filling in an empty object with
 * MinKey/MaxKey and stripping field names. Also translates keys according to the collation, if
 * necessary.
 *
 * See comment for finishMinObj() for why we need both 'minObj' and 'maxObj'.
 */
static BSONObj finishMaxObj(const IndexEntry& indexEntry,
                            const BSONObj& minObj,
                            const BSONObj& maxObj) {
    BSONObjBuilder bob;
    bob.appendMaxKey("");
    BSONObj maxKey = bob.obj();

    if (maxObj.isEmpty()) {
        if (0 < maxKey.woCompare(minObj, indexEntry.keyPattern, false)) {
            BSONObjBuilder maxKeyBuilder;
            maxKeyBuilder.appendMaxKey("");
            return maxKeyBuilder.obj();
        } else {
            BSONObjBuilder minKeyBuilder;
            minKeyBuilder.appendMinKey("");
            return minKeyBuilder.obj();
        }
    } else {
        return stripFieldNamesAndApplyCollation(maxObj, indexEntry.collator);
    }
}

//QueryPlanner::plan中调用  
//没有匹配的索引则需要全表扫描，通过buildCollscanSoln生成查询计划，有合适的索引则QueryPlannerAnalysis::analyzeDataAccess生成
QuerySolution* buildCollscanSoln(const CanonicalQuery& query,
                                 bool tailable,
                                 const QueryPlannerParams& params) {
    std::unique_ptr<QuerySolutionNode> solnRoot(
        QueryPlannerAccess::makeCollectionScan(query, tailable, params));
    return QueryPlannerAnalysis::analyzeDataAccess(query, params, std::move(solnRoot));
}


//QueryPlanner::plan
QuerySolution* buildWholeIXSoln(const IndexEntry& index,
                                const CanonicalQuery& query,
                                const QueryPlannerParams& params,
                                int direction = 1) {
    std::unique_ptr<QuerySolutionNode> solnRoot(
        QueryPlannerAccess::scanWholeIndex(index, query, params, direction));
    return QueryPlannerAnalysis::analyzeDataAccess(query, params, std::move(solnRoot));
}

bool providesSort(const CanonicalQuery& query, const BSONObj& kp) {
    return query.getQueryRequest().getSort().isPrefixOf(kp, SimpleBSONElementComparator::kInstance);
}

// static
const int QueryPlanner::kPlannerVersion = 1;

//根据候选索引和MatchExpression生成PlanCacheIndexTree
//QueryPlanner::plan中调用
Status QueryPlanner::cacheDataFromTaggedTree(const MatchExpression* const taggedTree,
                                             const vector<IndexEntry>& relevantIndices,
                                             PlanCacheIndexTree** out) {
    // On any early return, the out-parameter must contain NULL.
    *out = NULL;

    if (NULL == taggedTree) {
        return Status(ErrorCodes::BadValue, "Cannot produce cache data: tree is NULL.");
    }

    unique_ptr<PlanCacheIndexTree> indexTree(new PlanCacheIndexTree());

    if (taggedTree->getTag() &&
        taggedTree->getTag()->getType() == MatchExpression::TagData::Type::IndexTag) {
        IndexTag* itag = static_cast<IndexTag*>(taggedTree->getTag());
        if (itag->index >= relevantIndices.size()) {
            mongoutils::str::stream ss;
            ss << "Index number is " << itag->index << " but there are only "
               << relevantIndices.size() << " relevant indices.";
            return Status(ErrorCodes::BadValue, ss);
        }

        // Make sure not to cache solutions which use '2d' indices.
        // A 2d index that doesn't wrap on one query may wrap on another, so we have to
        // check that the index is OK with the predicate. The only thing we have to do
        // this for is 2d.  For now it's easier to move ahead if we don't cache 2d.
        //
        // TODO: revisit with a post-cached-index-assignment compatibility check
        if (is2DIndex(relevantIndices[itag->index].keyPattern)) {
            return Status(ErrorCodes::BadValue, "can't cache '2d' index");
        }

        IndexEntry* ientry = new IndexEntry(relevantIndices[itag->index]);
        indexTree->entry.reset(ientry);
        indexTree->index_pos = itag->pos;
        indexTree->canCombineBounds = itag->canCombineBounds;
    } else if (taggedTree->getTag() &&
               taggedTree->getTag()->getType() == MatchExpression::TagData::Type::OrPushdownTag) {
        OrPushdownTag* orPushdownTag = static_cast<OrPushdownTag*>(taggedTree->getTag());

        if (orPushdownTag->getIndexTag()) {
            const IndexTag* itag = static_cast<const IndexTag*>(orPushdownTag->getIndexTag());

            if (is2DIndex(relevantIndices[itag->index].keyPattern)) {
                return Status(ErrorCodes::BadValue, "can't cache '2d' index");
            }

            std::unique_ptr<IndexEntry> indexEntry =
                stdx::make_unique<IndexEntry>(relevantIndices[itag->index]);
            indexTree->entry.reset(indexEntry.release());
            indexTree->index_pos = itag->pos;
            indexTree->canCombineBounds = itag->canCombineBounds;
        }

        for (const auto& dest : orPushdownTag->getDestinations()) {
            PlanCacheIndexTree::OrPushdown orPushdown;
            orPushdown.route = dest.route;
            IndexTag* indexTag = static_cast<IndexTag*>(dest.tagData.get());
            orPushdown.indexName = relevantIndices[indexTag->index].name;
            orPushdown.position = indexTag->pos;
            orPushdown.canCombineBounds = indexTag->canCombineBounds;
            indexTree->orPushdowns.push_back(std::move(orPushdown));
        }
    }

    for (size_t i = 0; i < taggedTree->numChildren(); ++i) {
        MatchExpression* taggedChild = taggedTree->getChild(i);
        PlanCacheIndexTree* indexTreeChild;
        Status s = cacheDataFromTaggedTree(taggedChild, relevantIndices, &indexTreeChild);
        if (!s.isOK()) {
            return s;
        }
        indexTree->children.push_back(indexTreeChild);
    }

    *out = indexTree.release();
    return Status::OK();
}

// static
Status QueryPlanner::tagAccordingToCache(MatchExpression* filter,
                                         const PlanCacheIndexTree* const indexTree,
                                         const map<StringData, size_t>& indexMap) {
    if (NULL == filter) {
        return Status(ErrorCodes::BadValue, "Cannot tag tree: filter is NULL.");
    }
    if (NULL == indexTree) {
        return Status(ErrorCodes::BadValue, "Cannot tag tree: indexTree is NULL.");
    }

    // We're tagging the tree here, so it shouldn't have
    // any tags hanging off yet.
    verify(NULL == filter->getTag());

    if (filter->numChildren() != indexTree->children.size()) {
        mongoutils::str::stream ss;
        ss << "Cache topology and query did not match: "
           << "query has " << filter->numChildren() << " children "
           << "and cache has " << indexTree->children.size() << " children.";
        return Status(ErrorCodes::BadValue, ss);
    }

    // Continue the depth-first tree traversal.
    for (size_t i = 0; i < filter->numChildren(); ++i) {
        Status s = tagAccordingToCache(filter->getChild(i), indexTree->children[i], indexMap);
        if (!s.isOK()) {
            return s;
        }
    }

    if (!indexTree->orPushdowns.empty()) {
        filter->setTag(new OrPushdownTag());
        OrPushdownTag* orPushdownTag = static_cast<OrPushdownTag*>(filter->getTag());
        for (const auto& orPushdown : indexTree->orPushdowns) {
            auto index = indexMap.find(orPushdown.indexName);
            if (index == indexMap.end()) {
                return Status(ErrorCodes::BadValue,
                              str::stream() << "Did not find index with name: "
                                            << orPushdown.indexName);
            }
            OrPushdownTag::Destination dest;
            dest.route = orPushdown.route;
            dest.tagData = stdx::make_unique<IndexTag>(
                index->second, orPushdown.position, orPushdown.canCombineBounds);
            orPushdownTag->addDestination(std::move(dest));
        }
    }

    if (indexTree->entry.get()) {
        map<StringData, size_t>::const_iterator got = indexMap.find(indexTree->entry->name);
        if (got == indexMap.end()) {
            mongoutils::str::stream ss;
            ss << "Did not find index with name: " << indexTree->entry->name;
            return Status(ErrorCodes::BadValue, ss);
        }
        if (filter->getTag()) {
            OrPushdownTag* orPushdownTag = static_cast<OrPushdownTag*>(filter->getTag());
            orPushdownTag->setIndexTag(
                new IndexTag(got->second, indexTree->index_pos, indexTree->canCombineBounds));
        } else {
            filter->setTag(
                new IndexTag(got->second, indexTree->index_pos, indexTree->canCombineBounds));
        }
    }

    return Status::OK();
}
//注意QueryPlanner::plan和QueryPlanner::planFromCache的区别
// static   prepareExecution中执行，从plancache中获取QuerySolution
Status QueryPlanner::planFromCache(const CanonicalQuery& query,
                                   const QueryPlannerParams& params,
                                   const CachedSolution& cachedSoln,
                                   QuerySolution** out) {
    invariant(!cachedSoln.plannerData.empty());
    invariant(out);

    // A query not suitable for caching should not have made its way into the cache.
    invariant(PlanCache::shouldCacheQuery(query));

    // Look up winning solution in cached solution's array.
    const SolutionCacheData& winnerCacheData = *cachedSoln.plannerData[0];

    if (SolutionCacheData::WHOLE_IXSCAN_SOLN == winnerCacheData.solnType) {
        // The solution can be constructed by a scan over the entire index.
        QuerySolution* soln = buildWholeIXSoln(
            *winnerCacheData.tree->entry, query, params, winnerCacheData.wholeIXSolnDir);
        if (soln == NULL) {
            return Status(ErrorCodes::BadValue,
                          "plan cache error: soln that uses index to provide sort");
        } else {
            *out = soln;
            return Status::OK();
        }
    } else if (SolutionCacheData::COLLSCAN_SOLN == winnerCacheData.solnType) {
        // The cached solution is a collection scan. We don't cache collscans
        // with tailable==true, hence the false below.
        QuerySolution* soln = buildCollscanSoln(query, false, params);
        if (soln == NULL) {
            return Status(ErrorCodes::BadValue, "plan cache error: collection scan soln");
        } else {
            *out = soln;
            return Status::OK();
        }
    }

    // SolutionCacheData::USE_TAGS_SOLN == cacheData->solnType
    // If we're here then this is neither the whole index scan or collection scan
    // cases, and we proceed by using the PlanCacheIndexTree to tag the query tree.

    // Create a copy of the expression tree.  We use cachedSoln to annotate this with indices.
    unique_ptr<MatchExpression> clone = query.root()->shallowClone();

    LOG(2) << "Tagging the match expression according to cache data: " << endl
           << "Filter:" << endl
           << redact(clone->toString()) << "Cache data:" << endl
           << redact(winnerCacheData.toString());

    // Map from index name to index number.
    // TODO: can we assume that the index numbering has the same lifetime
    // as the cache state?
    map<StringData, size_t> indexMap;
    for (size_t i = 0; i < params.indices.size(); ++i) {
        const IndexEntry& ie = params.indices[i];
        indexMap[ie.name] = i;
        LOG(2) << "Index " << i << ": " << ie.name;
    }

    Status s = tagAccordingToCache(clone.get(), winnerCacheData.tree.get(), indexMap);
    if (!s.isOK()) {
        return s;
    }

    // The MatchExpression tree is in canonical order. We must order the nodes for access planning.
    prepareForAccessPlanning(clone.get());

    LOG(2) << "Tagged tree:" << endl << redact(clone->toString());

    // Use the cached index assignments to build solnRoot.
    std::unique_ptr<QuerySolutionNode> solnRoot(QueryPlannerAccess::buildIndexedDataAccess(
        query, clone.release(), false, params.indices, params));

    if (!solnRoot) {
        return Status(ErrorCodes::BadValue,
                      str::stream() << "Failed to create data access plan from cache. Query: "
                                    << query.toStringShort());
    }

    // Takes ownership of 'solnRoot'.
    QuerySolution* soln =
        QueryPlannerAnalysis::analyzeDataAccess(query, params, std::move(solnRoot));
    if (!soln) {
        return Status(ErrorCodes::BadValue,
                      str::stream() << "Failed to analyze plan from cache. Query: "
                                    << query.toStringShort());
    }

    LOG(2) << "Planner: solution constructed from the cache:\n" << redact(soln->toString());
    *out = soln;
    return Status::OK();
}

/*
(gdb) bt
#0  mongo::QueryPlanner::plan (query=..., params=..., out=out@entry=0x7f2a4c4e8460) at src/mongo/db/query/query_planner.cpp:547
#1  0x00007f2a4d474225 in mongo::(anonymous namespace)::prepareExecution (opCtx=opCtx@entry=0x7f2a54621900, collection=collection@entry=0x7f2a50bdf340, ws=0x7f2a5461f580, canonicalQuery=..., plannerOptions=plannerOptions@entry=0)
    at src/mongo/db/query/get_executor.cpp:385
#2  0x00007f2a4d47927e in mongo::getExecutor (opCtx=opCtx@entry=0x7f2a54621900, collection=collection@entry=0x7f2a50bdf340, canonicalQuery=..., yieldPolicy=yieldPolicy@entry=mongo::PlanExecutor::YIELD_AUTO, 
    plannerOptions=plannerOptions@entry=0) at src/mongo/db/query/get_executor.cpp:476
#3  0x00007f2a4d4794fb in mongo::getExecutorFind (opCtx=opCtx@entry=0x7f2a54621900, collection=collection@entry=0x7f2a50bdf340, nss=..., canonicalQuery=..., yieldPolicy=yieldPolicy@entry=mongo::PlanExecutor::YIELD_AUTO, 
    plannerOptions=0) at src/mongo/db/query/get_executor.cpp:674
#4  0x00007f2a4d0ec613 in mongo::(anonymous namespace)::FindCmd::run (this=this@entry=0x7f2a4f3c9740 <mongo::(anonymous namespace)::findCmd>, opCtx=opCtx@entry=0x7f2a54621900, dbname=..., cmdObj=..., result=...)
    at src/mongo/db/commands/find_cmd.cpp:311
#5  0x00007f2a4e11fb36 in mongo::BasicCommand::enhancedRun (this=0x7f2a4f3c9740 <mongo::(anonymous namespace)::findCmd>, opCtx=0x7f2a54621900, request=..., result=...) at src/mongo/db/commands.cpp:416
#6  0x00007f2a4e11c2df in mongo::Command::publicRun (this=0x7f2a4f3c9740 <mongo::(anonymous namespace)::findCmd>, opCtx=0x7f2a54621900, request=..., result=...) at src/mongo/db/commands.cpp:354
#7  0x00007f2a4d0981f4 in runCommandImpl (startOperationTime=..., replyBuilder=0x7f2a5487e8d0, request=..., command=0x7f2a4f3c9740 <mongo::(anonymous namespace)::findCmd>, opCtx=0x7f2a54621900)
    at src/mongo/db/service_entry_point_mongod.cpp:481
#8  mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7f2a54621900, command=command@entry=0x7f2a4f3c9740 <mongo::(anonymous namespace)::findCmd>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:757
#9  0x00007f2a4d09936f in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7f2a4c4e9400) at src/mongo/db/service_entry_point_mongod.cpp:878
#10 0x00007f2a4d09936f in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#11 0x00007f2a4d09a1d1 in runCommands (message=..., opCtx=0x7f2a54621900) at src/mongo/db/service_entry_point_mongod.cpp:888
#12 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7f2a54621900, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1161
#13 0x00007f2a4d0a6b0a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f2a5460a510, guard=...) at src/mongo/transport/service_state_machine.cpp:363
#14 0x00007f2a4d0a1c4f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f2a5460a510, guard=...) at src/mongo/transport/service_state_machine.cpp:423
#15 0x00007f2a4d0a568e in operator() (__closure=0x7f2a546690a0) at src/mongo/transport/service_state_machine.cpp:462
#16 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#17 0x00007f2a4dfe17e2 in operator() (this=0x7f2a4c4eb550) at /usr/local/include/c++/5.4.0/functional:2267
#18 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f2a50ddc480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:118
#19 0x00007f2a4d0a084d in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f2a5460a510, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:466
#20 0x00007f2a4d0a31e1 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7f2a5460a510, status=...) at src/mongo/transport/service_state_machine.cpp:291
#21 0x00007f2a4d0a3ddb in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7f2a5460a510, guard=...) at src/mongo/transport/service_state_machine.cpp:250
#22 0x00007f2a4d0a1ce1 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f2a5460a510, guard=...) at src/mongo/transport/service_state_machine.cpp:420
#23 0x00007f2a4d0a568e in operator() (__closure=0x7f2a50de5da0) at src/mongo/transport/service_state_machine.cpp:462
#24 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#25 0x00007f2a4dfe1d45 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#26 operator() (__closure=0x7f2a545dfbf0) at src/mongo/transport/service_executor_synchronous.cpp:135
#27 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#28 0x00007f2a4e531894 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#29 mongo::(anonymous namespace)::runFunc (ctx=0x7f2a546694c0) at src/mongo/transport/service_entry_point_utils.cpp:55
#30 0x00007f2a4b206e25 in start_thread () from /lib64/libpthread.so.0
#31 0x00007f2a4af3434d in clone () from /lib64/libc.so.6
*/
//参考https://yq.aliyun.com/articles/647563?spm=a2c4e.11155435.0.0.7cb74df3gUVck4 MongoDB 执行计划 & 优化器简介 (上)
//https://yq.aliyun.com/articles/74635	MongoDB查询优化：从 10s 到 10ms
//执行计划http://mongoing.com/archives/5624?spm=a2c4e.11153940.blogcont647563.13.6ee0730cDKb7RN 深入解析 MongoDB Plan Cache

// static   prepareExecution中调用  调用QueryPlanner::plan生成查询计划,这将会生成一个或者多个查询计划QuerySolution.

//根据已有索引生成对应QuerySolution存放到out数组中
//注意QueryPlanner::plan和QueryPlanner::planFromCache的区别
Status QueryPlanner::plan(const CanonicalQuery& query,
                          const QueryPlannerParams& params,
                          std::vector<QuerySolution*>* out) {
	/* db.test.find({"name":"yangyazhou", "age":22}).sort({"name":1})对应输出
	Options = INDEX_INTERSECTION SPLIT_LIMITED_SORT CANNOT_TRIM_IXISECT 
	Canonical query:
	ns=testxx.testTree: $and  //-------> and是因为查询条件是"name":"yangyazhou" 同时(and) "age":22
		age == 22.0
		name == "yangyazhou"
	Sort: { name: 1.0 }
	Proj: {}
	=============================
	*/
	LOG(2) << "Beginning planning..." << endl
           << "=============================" << endl
           << "Options = " << optionString(params.options) << endl
           << "Canonical query:" << endl
           << redact(query.toString()) << "============================="; //CanonicalQuery::toString

	//打印出所有索引信息  db.test.find({"name":"yangyazhou"}).sort({"name":1})
	//2019-01-03T17:24:40.440+0800 D QUERY    [conn1] Index 0 is kp: { _id: 1 } unique name: '_id_' io: { v: 2, key: { _id: 1 }, name: "_id_", ns: "testxx.test" }
	//2019-01-03T17:24:40.440+0800 D QUERY    [conn1] Index 1 is kp: { name: 1.0 } name: 'name_1' io: { v: 2, key: { name: 1.0 }, name: "name_1", ns: "testxx.test" }
	//2019-01-03T17:24:40.440+0800 D QUERY    [conn1] Index 2 is kp: { age: 1.0 } name: 'age_1' io: { v: 2, key: { age: 1.0 }, name: "age_1", ns: "testxx.test" }
	//2019-01-03T17:24:40.440+0800 D QUERY    [conn1] Index 3 is kp: { name: 1.0, age: 1.0 } name: 'name_1_age_1' io: { v: 2, key: { name: 1.0, age: 1.0 }, name: "name_1_age_1", ns: "testxx.test" }for (size_t i = 0; i < params.indices.size(); ++i) {
    for (size_t i = 0; i < params.indices.size(); ++i) {
        LOG(2) << "Index " << i << " is " << params.indices[i].toString();
    }

	//是否支持全部扫描
    const bool canTableScan = !(params.options & QueryPlannerParams::NO_TABLE_SCAN);
    const bool isTailable = query.getQueryRequest().isTailable();

    // If the query requests a tailable cursor, the only solution is a collscan + filter with
    // tailable set on the collscan.  TODO: This is a policy departure.  Previously I think you
    // could ask for a tailable cursor and it just tried to give you one.  Now, we fail if we
    // can't provide one.  Is this what we want?
    if (isTailable) {//Tailable Cursors相关请求走这里，固定集合才用
        if (!QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR) && canTableScan) {
            QuerySolution* soln = buildCollscanSoln(query, isTailable, params);
            if (NULL != soln) {
                out->push_back(soln);
            }
        }
        return Status::OK();
    }

    // The hint or sort can be $natural: 1.  If this happens, output a collscan. If both
    // a $natural hint and a $natural sort are specified, then the direction of the collscan
    // is determined by the sign of the sort (not the sign of the hint).

	//You can specify { $natural : 1 } to force the query to perform a forwards collection scan:
	//	db.users.find().hint( { $natural : 1 } )
	/*
	sort(_id:1) 和 sort($natural:1)排序区别
	_id 排序：按照插入顺序排序
	$natural 排序：按照数据在磁盘上的组织顺序排序
	参考:https://blog.csdn.net/qq_33961117/article/details/91416031
	*/
	//强制走$natural索引，或者$natural排序
    if (!query.getQueryRequest().getHint().isEmpty() ||
        !query.getQueryRequest().getSort().isEmpty()) {
        BSONObj hintObj = query.getQueryRequest().getHint();
        BSONObj sortObj = query.getQueryRequest().getSort();
        BSONElement naturalHint = dps::extractElementAtPath(hintObj, "$natural");
        BSONElement naturalSort = dps::extractElementAtPath(sortObj, "$natural");

        // A hint overrides a $natural sort. This means that we don't force a table
        // scan if there is a $natural sort with a non-$natural hint.
        if (!naturalHint.eoo() || (!naturalSort.eoo() && hintObj.isEmpty())) {
            LOG(2) << "Forcing a table scan due to hinted $natural";
            // min/max are incompatible with $natural.
            if (canTableScan && query.getQueryRequest().getMin().isEmpty() &&
                query.getQueryRequest().getMax().isEmpty()) {
                QuerySolution* soln = buildCollscanSoln(query, isTailable, params);
                if (NULL != soln) {
                    out->push_back(soln);
                }
            }
            return Status::OK();
        }
    }

    // Figure out what fields we care about.
    unordered_set<string> fields;
	////获取所有的查询条件，填充到fields数组 
    QueryPlannerIXSelect::getFields(query.root(), "", &fields);

	/* 如果是db.test.find({"name":"yangyazhou", "age":22}).sort({"name":1})
	2019-01-03T16:58:51.444+0800 D QUERY	[conn1] Predicate over field 'name'
	2019-01-03T16:58:51.444+0800 D QUERY	[conn1] Predicate over field 'age'

	如果是db.test.find({"name":"yangyazhou"}).sort({"name":1})
	2019-01-03T17:24:40.440+0800 D QUERY    [conn1] Predicate over field 'name'
	*/
    for (unordered_set<string>::const_iterator it = fields.begin(); it != fields.end(); ++it) {
        LOG(2) << "Predicate over field '" << *it << "'"; 
		//查找的条件，如db.sbtest1.find({"k":2927256})，这里为 Predicate over field 'k'
    }

    // Filter our indices so we only look at indices that are over our predicates.
    //选出可能匹配的索引存到这里
    vector<IndexEntry> relevantIndices;

    // Hints require us to only consider the hinted index.
    // If index filters in the query settings were used to override
    // the allowed indices for planning, we should not use the hinted index
    // requested in the query.
    BSONObj hintIndex;
	//如果indexFiltersApplied为false，hint才有用
    if (!params.indexFiltersApplied) { 
        hintIndex = query.getQueryRequest().getHint();
    }

    // If snapshot is set, default to collscanning. If the query param SNAPSHOT_USE_ID is set,
    // snapshot is a form of a hint, so try to use _id index to make a real plan. If that fails,
    // just scan the _id index.
    //
    // Don't do this if the query is a geonear or text as as text search queries must be answered
    // using full text indices and geoNear queries must be answered using geospatial indices.
    if (query.getQueryRequest().isSnapshot()) {//查询带Snapshot
        RARELY {
            warning() << "The snapshot option is deprecated. See "
                         "http://dochub.mongodb.org/core/snapshot-deprecation";
        }

        if (!QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR) &&
            !QueryPlannerCommon::hasNode(query.root(), MatchExpression::TEXT)) {
            const bool useIXScan = params.options & QueryPlannerParams::SNAPSHOT_USE_ID;

            if (!useIXScan) {
                QuerySolution* soln = buildCollscanSoln(query, isTailable, params);
                if (soln) {
                    out->push_back(soln);
                }
                return Status::OK();
            } else {
                // Find the ID index in indexKeyPatterns. It's our hint.
                for (size_t i = 0; i < params.indices.size(); ++i) {
                    if (isIdIndex(params.indices[i].keyPattern)) {
                        hintIndex = params.indices[i].keyPattern;
                        break;
                    }
                }
            }
        }
    }

    boost::optional<size_t> hintIndexNumber;

    if (hintIndex.isEmpty()) { //如果没有强制指定索引
    /*
	db.test.find({"name":"yangyazhou", "age":1, "male":1})

	外层选举出的out索引打印如下:
	2021-01-12T17:57:31.001+0800 D QUERY    [conn1] Relevant index 0 is kp: { name: 1.0 } name: 'name_1' io: { v: 2, key: { name: 1.0 }, name: "name_1", ns: "test.test", background: true }
	2021-01-12T17:57:31.001+0800 D QUERY    [conn1] Relevant index 1 is kp: { age: 1.0 } name: 'age_1' io: { v: 2, key: { age: 1.0 }, name: "age_1", ns: "test.test", background: true }
	2021-01-12T17:57:31.001+0800 D QUERY    [conn1] Relevant index 2 is kp: { male: 1.0 } name: 'male_1' io: { v: 2, key: { male: 1.0 }, name: "male_1", ns: "test.test", background: true }
	2021-01-12T17:57:31.001+0800 D QUERY    [conn1] Relevant index 3 is kp: { male: 1.0, name: 1.0 } name: 'male_1_name_1' io: { v: 2, key: { male: 1.0, name: 1.0 }, name: "male_1_name_1", ns: "test.test", background: true }
	2021-01-12T17:57:31.001+0800 D QUERY    [conn1] Relevant index 4 is kp: { name: 1.0, male: 1.0 } name: 'name_1_male_1' io: { v: 2, key: { name: 1.0, male: 1.0 }, name: "name_1_male_1", ns: "test.test", background: true }
	*/
		//获取满足条件的索引存储到relevantIndices  把和fields匹配的索引找出来,最左的合理索引，都会选出来
        QueryPlannerIXSelect::findRelevantIndices(fields, params.indices, &relevantIndices);
    } else {
        // Sigh.  If the hint is specified it might be using the index name.
        BSONElement firstHintElt = hintIndex.firstElement();
        if (str::equals("$hint", firstHintElt.fieldName()) && String == firstHintElt.type()) {
            string hintName = firstHintElt.String();
            for (size_t i = 0; i < params.indices.size(); ++i) {
                if (params.indices[i].name == hintName) {
                    LOG(2) << "Hint by name specified, restricting indices to "
                           << params.indices[i].keyPattern.toString();
                    relevantIndices.clear();
                    relevantIndices.push_back(params.indices[i]);
                    hintIndexNumber = i;
                    hintIndex = params.indices[i].keyPattern;
                    break;
                }
            }
        } else {
            for (size_t i = 0; i < params.indices.size(); ++i) {
                if (0 == params.indices[i].keyPattern.woCompare(hintIndex)) {
                    relevantIndices.clear();
                    relevantIndices.push_back(params.indices[i]);
                    LOG(2) << "Hint specified, restricting indices to " << hintIndex.toString();
                    if (hintIndexNumber) {
                        return Status(ErrorCodes::IndexNotFound,
                                      str::stream() << "Hint matched multiple indexes, "
                                                    << "must hint by index name. Matched: "
                                                    << params.indices[i].toString()
                                                    << " and "
                                                    << params.indices[*hintIndexNumber].toString());
                    }
                    hintIndexNumber = i;
                }
            }
        }

        if (!hintIndexNumber) {
            return Status(ErrorCodes::BadValue, "bad hint");
        }
    }

    // Deal with the .min() and .max() query options.  If either exist we can only use an index
    // that matches the object inside.
    if (!query.getQueryRequest().getMin().isEmpty() ||  //MIN MAX操作符相关
        !query.getQueryRequest().getMax().isEmpty()) {
        BSONObj minObj = query.getQueryRequest().getMin();
        BSONObj maxObj = query.getQueryRequest().getMax();

        // The unfinished siblings of these objects may not be proper index keys because they
        // may be empty objects or have field names. When an index is picked to use for the
        // min/max query, these "finished" objects will always be valid index keys for the
        // index's key pattern.
        BSONObj finishedMinObj;
        BSONObj finishedMaxObj;

        // This is the index into params.indices[...] that we use.
        size_t idxNo = numeric_limits<size_t>::max();

        // If there's an index hinted we need to be able to use it.
        if (!hintIndex.isEmpty()) { //强制索引
            invariant(hintIndexNumber);
            const auto& hintedIndexEntry = params.indices[*hintIndexNumber];

            if (!minObj.isEmpty() &&
                !indexCompatibleMaxMin(minObj, query.getCollator(), hintedIndexEntry)) {
                LOG(2) << "Minobj doesn't work with hint";
                return Status(ErrorCodes::BadValue, "hint provided does not work with min query");
            }

            if (!maxObj.isEmpty() &&
                !indexCompatibleMaxMin(maxObj, query.getCollator(), hintedIndexEntry)) {
                LOG(2) << "Maxobj doesn't work with hint";
                return Status(ErrorCodes::BadValue, "hint provided does not work with max query");
            }

            finishedMinObj = finishMinObj(hintedIndexEntry, minObj, maxObj);
            finishedMaxObj = finishMaxObj(hintedIndexEntry, minObj, maxObj);

            // The min must be less than the max for the hinted index ordering.
            if (0 <= finishedMinObj.woCompare(finishedMaxObj, hintedIndexEntry.keyPattern, false)) {
                LOG(2) << "Minobj/Maxobj don't work with hint";
                return Status(ErrorCodes::BadValue,
                              "hint provided does not work with min/max query");
            }

            idxNo = *hintIndexNumber;
        } else {
            // No hinted index, look for one that is compatible (has same field names and
            // ordering thereof).
            for (size_t i = 0; i < params.indices.size(); ++i) {
                const auto& indexEntry = params.indices[i];

                BSONObj toUse = minObj.isEmpty() ? maxObj : minObj;
                if (indexCompatibleMaxMin(toUse, query.getCollator(), indexEntry)) {
                    // In order to be fully compatible, the min has to be less than the max
                    // according to the index key pattern ordering. The first step in verifying
                    // this is "finish" the min and max by replacing empty objects and stripping
                    // field names.
                    finishedMinObj = finishMinObj(indexEntry, minObj, maxObj);
                    finishedMaxObj = finishMaxObj(indexEntry, minObj, maxObj);

                    // Now we have the final min and max. This index is only relevant for
                    // the min/max query if min < max.
                    if (0 >=
                        finishedMinObj.woCompare(finishedMaxObj, indexEntry.keyPattern, false)) {
                        // Found a relevant index.
                        idxNo = i;
                        break;
                    }

                    // This index is not relevant; move on to the next.
                }
            }
        }

        if (idxNo == numeric_limits<size_t>::max()) {
            LOG(2) << "Can't find relevant index to use for max/min query";
            // Can't find an index to use, bail out.
            return Status(ErrorCodes::BadValue, "unable to find relevant index for max/min query");
        }

        LOG(2) << "Max/min query using index " << params.indices[idxNo].toString();

        // Make our scan and output.
        std::unique_ptr<QuerySolutionNode> solnRoot(QueryPlannerAccess::makeIndexScan(
            params.indices[idxNo], query, params, finishedMinObj, finishedMaxObj));
        invariant(solnRoot);

        QuerySolution* soln =
            QueryPlannerAnalysis::analyzeDataAccess(query, params, std::move(solnRoot));
        if (NULL != soln) {
            out->push_back(soln);
        }

        return Status::OK();
    } ////MIN MAX操作符相关条件这里结束


	//打印出选择出的索引
    for (size_t i = 0; i < relevantIndices.size(); ++i) {
	/* db.test.find({"name":"yangyazhou", "age":22}).sort({"name":1})
	2019-01-03T17:57:20.793+0800 D QUERY	[conn1] Relevant index 0 is kp: { name: 1.0 } name: 'name_1' io: { v: 2, key: { name: 1.0 }, name: "name_1", ns: "testxx.test" }
	2019-01-03T17:57:20.793+0800 D QUERY	[conn1] Relevant index 1 is kp: { age: 1.0 } name: 'age_1' io: { v: 2, key: { age: 1.0 }, name: "age_1", ns: "testxx.test" }
	2019-01-03T17:57:20.793+0800 D QUERY	[conn1] Relevant index 2 is kp: { name: 1.0, age: 1.0 } name: 'name_1_age_1' io: { v: 2, key: { name: 1.0, age: 1.0 }, name: "name_1_age_1", ns: "testxx.test" }

    db.test.find({"name":"yangyazhou"}).sort({"name":1})
	2019-01-03T17:24:40.440+0800 D QUERY	[conn1] Relevant index 0 is kp: { name: 1.0 } name: 'name_1' io: { v: 2, key: { name: 1.0 }, name: "name_1", ns: "testxx.test" }
	2019-01-03T17:24:40.440+0800 D QUERY	[conn1] Relevant index 1 is kp: { name: 1.0, age: 1.0 } name: 'name_1_age_1' io: { v: 2, key: { name: 1.0, age: 1.0 }, name: "name_1_age_1", ns: "testxx.test" }
	*/
		LOG(2) << "Relevant index " << i << " is " << relevantIndices[i].toString();
    }

    // Figure out how useful each index is to each predicate.
    QueryPlannerIXSelect::rateIndices(query.root(), "", relevantIndices, query.getCollator());
    QueryPlannerIXSelect::stripInvalidAssignments(query.root(), relevantIndices);

    // Unless we have GEO_NEAR, TEXT, or a projection, we may be able to apply an optimization
    // in which we strip unnecessary index assignments.
    //
    // Disallowed with projection because assignment to a non-unique index can allow the plan
    // to be covered.
    //
    // TEXT and GEO_NEAR are special because they require the use of a text/geo index in order
    // to be evaluated correctly. Stripping these "mandatory assignments" is therefore invalid.
    if (query.getQueryRequest().getProj().isEmpty() &&
        !QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR) &&
        !QueryPlannerCommon::hasNode(query.root(), MatchExpression::TEXT)) {
        QueryPlannerIXSelect::stripUnneededAssignments(query.root(), relevantIndices);
    }

    // query.root() is now annotated with RelevantTag(s).
    /* 
    db.test.find({"name":"yangyazhou"}).sort({"name":1}):
		name == "yangyazhou"  || First: 0 1 notFirst: full path: name

	db.test.find({"name":"yangyazhou", "age":22}).sort({"name":1}):
		$and
	    age == 22.0  || First: 1 notFirst: 2 full path: age
	    name == "yangyazhou"  || First: 0 2 notFirst: full path: name
	*/
    LOG(2) << "Rated tree:" << endl << redact(query.root()->toString()); 

    // If there is a GEO_NEAR it must have an index it can use directly.
    const MatchExpression* gnNode = NULL;
	//GEO相关处理
    if (QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR, &gnNode)) {
        // No index for GEO_NEAR?  No query.
        RelevantTag* tag = static_cast<RelevantTag*>(gnNode->getTag());
        if (!tag || (0 == tag->first.size() && 0 == tag->notFirst.size())) {
            LOG(2) << "Unable to find index for $geoNear query.";
            // Don't leave tags on query tree.
            query.root()->resetTag();
            return Status(ErrorCodes::BadValue, "unable to find index for $geoNear query");
        }

        LOG(2) << "Rated tree after geonear processing:" << redact(query.root()->toString());
    }

    // Likewise, if there is a TEXT it must have an index it can use directly.
    const MatchExpression* textNode = NULL;
	//TEXT相关处理
    if (QueryPlannerCommon::hasNode(query.root(), MatchExpression::TEXT, &textNode)) {
        RelevantTag* tag = static_cast<RelevantTag*>(textNode->getTag());

        // Exactly one text index required for TEXT.  We need to check this explicitly because
        // the text stage can't be built if no text index exists or there is an ambiguity as to
        // which one to use.
        size_t textIndexCount = 0;
        for (size_t i = 0; i < params.indices.size(); i++) {
            if (INDEX_TEXT == params.indices[i].type) {
                textIndexCount++;
            }
        }
        if (textIndexCount != 1) {
            // Don't leave tags on query tree.
            query.root()->resetTag();
            return Status(ErrorCodes::BadValue, "need exactly one text index for $text query");
        }

        // Error if the text node is tagged with zero indices.
        if (0 == tag->first.size() && 0 == tag->notFirst.size()) {
            // Don't leave tags on query tree.
            query.root()->resetTag();
            return Status(ErrorCodes::BadValue,
                          "failed to use text index to satisfy $text query (if text index is "
                          "compound, are equality predicates given for all prefix fields?)");
        }

        // At this point, we know that there is only one text index and that the TEXT node is
        // assigned to it.
        invariant(1 == tag->first.size() + tag->notFirst.size());

        LOG(2) << "Rated tree after text processing:" << redact(query.root()->toString());
    }

    // If we have any relevant indices, we try to create indexed plans.
    //生成QuerySolution添加到out中
    if (0 < relevantIndices.size()) {
        // The enumerator spits out trees tagged with IndexTag(s).
        PlanEnumeratorParams enumParams;
        enumParams.intersect = params.options & QueryPlannerParams::INDEX_INTERSECTION;
        enumParams.root = query.root();
        enumParams.indices = &relevantIndices;

		//类PlanEnumerator 罗列MatchExpression的各种可能的组合， （indexScan & collectionScan等）， 生成具体的MatchExpression
        PlanEnumerator isp(enumParams);
        isp.init().transitional_ignore();
	
        unique_ptr<MatchExpression> rawTree;
		//根据CanonicalQuery和满足要求的索引relevantIndices来生成QuerySolution树
		//PlanEnumerator::getNext
        while ((rawTree = isp.getNext()) && (out->size() < params.maxIndexedSolutions)) {
		/* 
	    db.test.find({"name":"yangyazhou"}).sort({"name":1}):
			name == "yangyazhou"  || Selected Index #1 pos 0 combine 1

		db.test.find({"name":"yangyazhou", "age":22}).sort({"name":1}):
			$and
				age == 22.0  || Selected Index #2 pos 1 combine 1
				name == "yangyazhou"  || Selected Index #2 pos 0 combine 1
		*/
            LOG(2) << "About to build solntree(QuerySolution tree) from tagged tree:" << endl
                   << redact(rawTree.get()->toString());

            // Store the plan cache index tree before calling prepareForAccessingPlanning(), so that
            // the PlanCacheIndexTree has the same sort as the MatchExpression used to generate the
            // plan cache key.
            std::unique_ptr<MatchExpression> clone(rawTree.get()->shallowClone());
            PlanCacheIndexTree* cacheData;
			//根据候选索引和MatchExpression生成PlanCacheIndexTree
            Status indexTreeStatus = 
                cacheDataFromTaggedTree(clone.get(), relevantIndices, &cacheData);
            if (!indexTreeStatus.isOK()) {
                LOG(2) << "Query is not cachable: " << redact(indexTreeStatus.reason());
            }
            unique_ptr<PlanCacheIndexTree> autoData(cacheData);

            // We have already cached the tree in canonical order, so now we can order the nodes for
            // access planning.
            prepareForAccessPlanning(rawTree.get());

			//QueryPlannerAccess::buildIndexedDataAccess
            // This can fail if enumeration makes a mistake.
            //根据MatchExpression的节点的类型， 建立对应的QuerySolutionNode节点， 最终形成一个树形的QuerySolutionNode树
            std::unique_ptr<QuerySolutionNode> solnRoot(QueryPlannerAccess::buildIndexedDataAccess(
                query, rawTree.release(), false, relevantIndices, params));

            if (!solnRoot) {
                continue;
            }

			//获取对应QuerySolution
            QuerySolution* soln =
                QueryPlannerAnalysis::analyzeDataAccess(query, params, std::move(solnRoot));
            if (NULL != soln) {
                LOG(2) << "Planner: adding solution:" << endl << redact(soln->toString());//QuerySolutionNode::toString
                if (indexTreeStatus.isOK()) {
                    SolutionCacheData* scd = new SolutionCacheData();
                    scd->tree.reset(autoData.release());
                    soln->cacheData.reset(scd);
                }
				//QuerySolution添加到out中
                out->push_back(soln);
            }
        }
    }

    // Don't leave tags on query tree.
    query.root()->resetTag();

	//满足条件的索引个数
    LOG(2) << "Planner: outputted " << out->size() << " indexed solutions."; 

    // Produce legible error message for failed OR planning with a TEXT child.
    // TODO: support collection scan for non-TEXT children of OR.
    if (out->size() == 0 && textNode != NULL && MatchExpression::OR == query.root()->matchType()) {
        MatchExpression* root = query.root();
        for (size_t i = 0; i < root->numChildren(); ++i) {
            if (textNode == root->getChild(i)) {
                return Status(ErrorCodes::BadValue,
                              "Failed to produce a solution for TEXT under OR - "
                              "other non-TEXT clauses under OR have to be indexed as well.");
            }
        }
    }

    // An index was hinted.  If there are any solutions, they use the hinted index.  If not, we
    // scan the entire index to provide results and output that as our plan.  This is the
    // desired behavior when an index is hinted that is not relevant to the query.
    //走强制索引
    if (!hintIndex.isEmpty()) {
        if (0 == out->size()) {
            // Push hinted index solution to output list if found. It is possible to end up without
            // a solution in the case where a filtering QueryPlannerParams argument, such as
            // NO_BLOCKING_SORT, leads to its exclusion.
            if (auto soln = buildWholeIXSoln(params.indices[*hintIndexNumber], query, params)) {
                LOG(2) << "Planner: outputting soln that uses hinted index as scan.";
				LOG(2) << "Planner: outputting a buildWholeIXSoln:" << endl << redact(soln->toString());
                out->push_back(soln);
            }
        }
        return Status::OK();
    }

    // If a sort order is requested, there may be an index that provides it, even if that
    // index is not over any predicates in the query.
    //sort排序并且是GEO类型，同时带有TEXT
    if (!query.getQueryRequest().getSort().isEmpty() &&
        !QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR) &&
        !QueryPlannerCommon::hasNode(query.root(), MatchExpression::TEXT)) {
        // See if we have a sort provided from an index already.
        // This is implied by the presence of a non-blocking solution.
        bool usingIndexToSort = false;
        for (size_t i = 0; i < out->size(); ++i) {
            QuerySolution* soln = (*out)[i];
            if (!soln->hasBlockingStage) {
                usingIndexToSort = true;
                break;
            }
        }

        if (!usingIndexToSort) {
            for (size_t i = 0; i < params.indices.size(); ++i) {
                const IndexEntry& index = params.indices[i];
                // Only regular (non-plugin) indexes can be used to provide a sort, and only
                // non-sparse indexes can be used to provide a sort.
                //
                // TODO: Sparse indexes can't normally provide a sort, because non-indexed
                // documents could potentially be missing from the result set.  However, if the
                // query predicate can be used to guarantee that all documents to be returned
                // are indexed, then the index should be able to provide the sort.
                //
                // For example:
                // - Sparse index {a: 1, b: 1} should be able to provide a sort for
                //   find({b: 1}).sort({a: 1}).  SERVER-13908.
                // - Index {a: 1, b: "2dsphere"} (which is "geo-sparse", if
                //   2dsphereIndexVersion=2) should be able to provide a sort for
                //   find({b: GEO}).sort({a:1}).  SERVER-10801.
                if (index.type != INDEX_BTREE) {
                    continue;
                }
                if (index.sparse) {
                    continue;
                }

                // If the index collation differs from the query collation, the index should not be
                // used to provide a sort, because strings will be ordered incorrectly.
                if (!CollatorInterface::collatorsMatch(index.collator, query.getCollator())) {
                    continue;
                }

                // Partial indexes can only be used to provide a sort only if the query predicate is
                // compatible.
                if (index.filterExpr && !expression::isSubsetOf(query.root(), index.filterExpr)) {
                    continue;
                }

                const BSONObj kp = QueryPlannerAnalysis::getSortPattern(index.keyPattern);
                if (providesSort(query, kp)) {
                    LOG(2) << "Planner: outputting soln that uses index to provide sort.";
                    QuerySolution* soln = buildWholeIXSoln(params.indices[i], query, params);
                    if (NULL != soln) {
                        PlanCacheIndexTree* indexTree = new PlanCacheIndexTree();
                        indexTree->setIndexEntry(params.indices[i]);
                        SolutionCacheData* scd = new SolutionCacheData();
                        scd->tree.reset(indexTree);
                        scd->solnType = SolutionCacheData::WHOLE_IXSCAN_SOLN;
                        scd->wholeIXSolnDir = 1;
						LOG(2) << "Planner: outputting a buildWholeIXSoln:" << endl << redact(soln->toString());

                        soln->cacheData.reset(scd);
                        out->push_back(soln);
                        break;
                    }
                }
                if (providesSort(query, QueryPlannerCommon::reverseSortObj(kp))) {
                    LOG(2) << "Planner: outputting soln that uses (reverse) index "
                           << "to provide sort.";
                    QuerySolution* soln = buildWholeIXSoln(params.indices[i], query, params, -1);
                    if (NULL != soln) {
                        PlanCacheIndexTree* indexTree = new PlanCacheIndexTree();
                        indexTree->setIndexEntry(params.indices[i]);
                        SolutionCacheData* scd = new SolutionCacheData();
                        scd->tree.reset(indexTree);
                        scd->solnType = SolutionCacheData::WHOLE_IXSCAN_SOLN;
                        scd->wholeIXSolnDir = -1;
						LOG(2) << "Planner: outputting a buildWholeIXSoln:" << endl << redact(soln->toString());

                        soln->cacheData.reset(scd);
                        out->push_back(soln);
                        break;
                    }
                }
            }
        }
    }

    // If a projection exists, there may be an index that allows for a covered plan, even if none
    // were considered earlier.
    const auto projection = query.getProj();
    if (params.options & QueryPlannerParams::GENERATE_COVERED_IXSCANS && out->size() == 0 &&
        query.getQueryObj().isEmpty() && projection && !projection->requiresDocument()) {

        const auto* indicesToConsider = hintIndex.isEmpty() ? &params.indices : &relevantIndices;
        for (auto&& index : *indicesToConsider) {
            if (index.type != INDEX_BTREE || index.multikey || index.sparse || index.filterExpr ||
                !CollatorInterface::collatorsMatch(index.collator, query.getCollator())) {
                continue;
            }

            QueryPlannerParams paramsForCoveredIxScan;
            paramsForCoveredIxScan.options =
                params.options | QueryPlannerParams::NO_UNCOVERED_PROJECTIONS;
            auto soln = buildWholeIXSoln(index, query, paramsForCoveredIxScan);
            if (soln) {
                LOG(2) << "Planner: outputting soln that uses index to provide projection.";
				LOG(2) << "Planner: outputting a buildWholeIXSoln:" << endl << redact(soln->toString());
                PlanCacheIndexTree* indexTree = new PlanCacheIndexTree();
                indexTree->setIndexEntry(index);

                SolutionCacheData* scd = new SolutionCacheData();
                scd->tree.reset(indexTree);
                scd->solnType = SolutionCacheData::WHOLE_IXSCAN_SOLN;
                scd->wholeIXSolnDir = 1;
                soln->cacheData.reset(scd);

                out->push_back(soln);
                break;
            }
        }
    }

    // geoNear and text queries *require* an index.
    // Also, if a hint is specified it indicates that we MUST use it.
    bool possibleToCollscan =
        !QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR) &&
        !QueryPlannerCommon::hasNode(query.root(), MatchExpression::TEXT) && hintIndex.isEmpty();

    // The caller can explicitly ask for a collscan.
    bool collscanRequested = (params.options & QueryPlannerParams::INCLUDE_COLLSCAN);

    // No indexed plans?  We must provide a collscan if possible or else we can't run the query.
    //没有合适的索引并且允许全部扫描
    bool collscanNeeded = (0 == out->size() && canTableScan);

	//如果没有合适的QuerySolution，则进行全部扫描
    if (possibleToCollscan && (collscanRequested || collscanNeeded)) {
		//没有匹配的索引则需要全表扫描，通过buildCollscanSoln生成查询计划，上面如果有合适的索引则QueryPlannerAnalysis::analyzeDataAccess生成
        QuerySolution* collscan = buildCollscanSoln(query, isTailable, params);
        if (NULL != collscan) {
            SolutionCacheData* scd = new SolutionCacheData();
            scd->solnType = SolutionCacheData::COLLSCAN_SOLN;
            collscan->cacheData.reset(scd);
            out->push_back(collscan);
            LOG(2) << "Planner: outputting a collscan:" << endl << redact(collscan->toString());
        }
    }

    return Status::OK();
}

}  // namespace mongo
