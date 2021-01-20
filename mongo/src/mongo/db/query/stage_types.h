/**
 *    Copyright (C) 2013-2014 MongoDB Inc.
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

#pragma once

namespace mongo {

/**
 * These map to implementations of the PlanStage interface, all of which live in db/exec/
 */
enum StageType { //参考PlanStage* buildStages
    //对应QuerySolutionNode为AndHashNode
    STAGE_AND_HASH, //0
    //对应QuerySolutionNode为AndSortedNode
    STAGE_AND_SORTED,  //1
    STAGE_CACHED_PLAN, //2
    //CollectionScan::doWork 
    //对应QuerySolutionNode为CollectionScanNode
    STAGE_COLLSCAN,  //全表扫描   //例如CollectionScanNode赋值参考CollectionScanNode::getType

    // This stage sits at the root of the query tree and counts up the number of results
    // returned by its child.
    STAGE_COUNT, //4

    // If we're running a .count(), the query is fully covered by one ixscan, and the ixscan is
    // from one key to another, we can just skip through the keys without bothering to examine
    // them.
    STAGE_COUNT_SCAN,//5

    STAGE_DELETE,//6

    // If we're running a distinct, we only care about one value for each key.  The distinct
    // scan stage is an ixscan with some key-skipping behvaior that only distinct uses.
    //对应QuerySolutionNode为DistinctNode
    STAGE_DISTINCT_SCAN,//7

    // Dummy stage used for receiving notifications of deletions during chunk migration.
    STAGE_NOTIFY_DELETE,//8

    //对应QuerySolutionNode为EnsureSortedNode
    STAGE_ENSURE_SORTED,//9

    STAGE_EOF,//10

    // This is more of an "internal-only" stage where we try to keep docs that were mutated
    // during query execution.
    //对应QuerySolutionNode为KeepMutationsNode
    STAGE_KEEP_MUTATIONS,  
    //对应QuerySolutionNode为FetchNode
    STAGE_FETCH, //12  FetchStage::doWork

    // The two $geoNear impls imply a fetch+sort and must be stages.
    //对应QuerySolutionNode为GeoNear2DNode
    STAGE_GEO_NEAR_2D,
    //对应QuerySolutionNode为GeoNear2DSphereNode
    STAGE_GEO_NEAR_2DSPHERE,

    STAGE_GROUP, //15

    STAGE_IDHACK,

    // Simple wrapper to iterate a SortedDataInterface::Cursor.
    STAGE_INDEX_ITERATOR,

    ////对应QuerySolutionNode为IndexScanNode
    STAGE_IXSCAN,  //18 索引扫描，INDEX SCAN   IndexScan::doWork
    //对应QuerySolutionNode为LimitNode
    STAGE_LIMIT,

    // Implements parallelCollectionScan.
    STAGE_MULTI_ITERATOR, //20

    STAGE_MULTI_PLAN,  //21 MultiPlanStage
    STAGE_OPLOG_START,
    //对应QuerySolutionNode为OrNode
    STAGE_OR,
    //对应QuerySolutionNode为ProjectionNode
    STAGE_PROJECTION,

    // Stage for running aggregation pipelines.
    STAGE_PIPELINE_PROXY, //25

    STAGE_QUEUED_DATA,
    //对应QuerySolutionNode为ShardingFilterNode
    STAGE_SHARDING_FILTER,
    //对应QuerySolutionNode为SkipNode
    STAGE_SKIP,
    //对应QuerySolutionNode为SortNode
    STAGE_SORT,  //29 SortStage
    //对应QuerySolutionNode为SortKeyGeneratorNode
    STAGE_SORT_KEY_GENERATOR, //30  SortKeyGeneratorStage
    ////对应QuerySolutionNode为MergeSortNode
    STAGE_SORT_MERGE,
    STAGE_SUBPLAN,

    // Stages for running text search.
    //对应QuerySolutionNode为TextNode
    STAGE_TEXT,
    STAGE_TEXT_OR,
    STAGE_TEXT_MATCH, //35

    STAGE_UNKNOWN,

    STAGE_UPDATE,
};

}  // namespace mongo

