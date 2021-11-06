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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kQuery

#include "mongo/db/exec/collection_scan.h"

#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/database.h"
#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/exec/collection_scan_common.h"
#include "mongo/db/exec/filter.h"
#include "mongo/db/exec/scoped_timer.h"
#include "mongo/db/exec/working_set.h"
#include "mongo/db/exec/working_set_common.h"
#include "mongo/db/repl/optime.h"
#include "mongo/db/storage/record_fetcher.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"

#include "mongo/db/client.h"  // XXX-ERH

namespace mongo {

using std::unique_ptr;
using std::vector;
using stdx::make_unique;

/*
2021-01-22T10:59:08.080+0800 D QUERY    [conn-1] Winning solution:
FETCH  -------------这里PlanStage对应FetchStage   对应慢日志中的docsExamined:1
---fetched = 1
---sortedByDiskLoc = 0
---getSort = [{ age: 1 }, { name: 1 }, { name: 1, age: 1 }, ]
---Child:
------IXSCAN --------------这里PlanStage对应IndexScan  对应慢日志中的keysExamined:1 
---------indexName = name_1_age_1
keyPattern = { name: 1.0, age: 1.0 }
---------direction = 1
---------bounds = field #0['name']: ["yangyazhou", "yangyazhou"], field #1['age']: [MinKey, MaxKey]
---------fetched = 0
---------sortedByDiskLoc = 0
---------getSort = [{ age: 1 }, { name: 1 }, { name: 1, age: 1 }, ]

*/


// static
const char* CollectionScan::kStageType = "COLLSCAN";

/*
(gdb) bt
#0  mongo::CollectionScan::CollectionScan (this=0x7f644e182000, opCtx=<optimized out>, params=..., workingSet=<optimized out>, filter=<optimized out>) at src/mongo/db/exec/collection_scan.cpp:71
#1  0x00007f6445ab30d6 in mongo::buildStages (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498ce1e0, cq=..., qsol=..., root=<optimized out>, ws=ws@entry=0x7f644d32b180) at src/mongo/db/query/stage_builder.cpp:85
#2  0x00007f6445ab38df in mongo::buildStages (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498ce1e0, cq=..., qsol=..., root=0x7f644e149230, ws=ws@entry=0x7f644d32b180) at src/mongo/db/query/stage_builder.cpp:165
#3  0x00007f6445ab54b7 in mongo::StageBuilder::build (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498ce1e0, cq=..., solution=..., wsIn=wsIn@entry=0x7f644d32b180, rootOut=rootOut@entry=0x7f6444b0a400)
    at src/mongo/db/query/stage_builder.cpp:406
#4  0x00007f6445a9705b in mongo::(anonymous namespace)::prepareExecution (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498ce1e0, ws=0x7f644d32b180, canonicalQuery=..., plannerOptions=plannerOptions@entry=0)
    at src/mongo/db/query/get_executor.cpp:460
#5  0x00007f6445a9b3de in mongo::getExecutor (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498ce1e0, canonicalQuery=..., yieldPolicy=yieldPolicy@entry=mongo::PlanExecutor::YIELD_AUTO, 
    plannerOptions=plannerOptions@entry=0) at src/mongo/db/query/get_executor.cpp:524
#6  0x00007f6445a9b65b in mongo::getExecutorFind (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498ce1e0, nss=..., canonicalQuery=..., yieldPolicy=yieldPolicy@entry=mongo::PlanExecutor::YIELD_AUTO, 
    plannerOptions=0) at src/mongo/db/query/get_executor.cpp:729
#7  0x00007f644570e623 in mongo::(anonymous namespace)::FindCmd::run
*/ //buildStages  //buildStages中构造使用
CollectionScan::CollectionScan(OperationContext* opCtx,
                               const CollectionScanParams& params,
                               WorkingSet* workingSet,
                               const MatchExpression* filter)
    : PlanStage(kStageType, opCtx),
      _workingSet(workingSet),
      _filter(filter),
      _params(params),
      _isDead(false),
      _wsidForFetch(_workingSet->allocate()) {
    // Explain reports the direction of the collection scan.
    _specificStats.direction = params.direction;
    _specificStats.maxTs = params.maxTs;
    invariant(!_params.shouldTrackLatestOplogTimestamp || _params.collection->ns().isOplog());

    if (params.maxTs) {
        _endConditionBSON = BSON("$gte" << *(params.maxTs));
        _endCondition = stdx::make_unique<GTEMatchExpression>();
        invariantOK(_endCondition->init(repl::OpTime::kTimestampFieldName,
                                        _endConditionBSON.firstElement()));
    }
}

/*
#0  mongo::CollectionScan::doWork (this=0x7ffa9a401140, out=0x7ffa913668d0) at src/mongo/db/exec/collection_scan.cpp:82
#1  0x00007ffa9263064b in mongo::PlanStage::work (this=0x7ffa9a401140, out=out@entry=0x7ffa913668d0) at src/mongo/db/exec/plan_stage.cpp:73
#2  0x00007ffa923059da in mongo::PlanExecutor::getNextImpl (this=0x7ffa9a403e00, objOut=objOut@entry=0x7ffa913669d0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:611
#3  0x00007ffa923064eb in mongo::PlanExecutor::getNext (this=<optimized out>, objOut=objOut@entry=0x7ffa91366af0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:440
#4  0x00007ffa91f6ac55 in mongo::(anonymous namespace)::FindCmd::run (this=this@entry=0x7ffa94247740 <mongo::(anonymous namespace)::findCmd>, opCtx=opCtx@entry=0x7ffa9a401640, dbname=..., cmdObj=..., result=...)
    at src/mongo/db/commands/find_cmd.cpp:370


#0  mongo::CollectionScan::CollectionScan (this=0x7f644e182000, opCtx=<optimized out>, params=..., workingSet=<optimized out>, filter=<optimized out>) at src/mongo/db/exec/collection_scan.cpp:71
#1  0x00007f6445ab30d6 in mongo::buildStages (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498cde80, cq=..., qsol=..., root=<optimized out>, ws=ws@entry=0x7f644d32b600) at src/mongo/db/query/stage_builder.cpp:85
#2  0x00007f6445ab38df in mongo::buildStages (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498cde80, cq=..., qsol=..., root=0x7f644e13d080, ws=ws@entry=0x7f644d32b600) at src/mongo/db/query/stage_builder.cpp:165
#3  0x00007f6445ab54b7 in mongo::StageBuilder::build (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498cde80, cq=..., solution=..., wsIn=wsIn@entry=0x7f644d32b600, rootOut=rootOut@entry=0x7f6444b0a400)
    at src/mongo/db/query/stage_builder.cpp:406
#4  0x00007f6445a9705b in mongo::(anonymous namespace)::prepareExecution (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498cde80, ws=0x7f644d32b600, canonicalQuery=..., plannerOptions=plannerOptions@entry=0)
    at src/mongo/db/query/get_executor.cpp:460
#5  0x00007f6445a9b3de in mongo::getExecutor (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498cde80, canonicalQuery=..., yieldPolicy=yieldPolicy@entry=mongo::PlanExecutor::YIELD_AUTO, 
    plannerOptions=plannerOptions@entry=0) at src/mongo/db/query/get_executor.cpp:524
#6  0x00007f6445a9b65b in mongo::getExecutorFind (opCtx=opCtx@entry=0x7f644e182640, collection=collection@entry=0x7f64498cde80, nss=..., canonicalQuery=..., yieldPolicy=yieldPolicy@entry=mongo::PlanExecutor::YIELD_AUTO, 
    plannerOptions=0) at src/mongo/db/query/get_executor.cpp:729
#7  0x00007f644570e623 in mongo::(anonymous namespace)::FindCmd::run
*/ //全表扫描走这里面  //IndexScan::doWork(走索引)  CollectionScan::doWork(全表扫描)
PlanStage::StageState CollectionScan::doWork(WorkingSetID* out) {//PlanStage::work中执行
    if (_isDead) {
        Status status(
            ErrorCodes::CappedPositionLost,
            str::stream()
                << "CollectionScan died due to position in capped collection being deleted. "
                << "Last seen record id: "
                << _lastSeenId);
        *out = WorkingSetCommon::allocateStatusMember(_workingSet, status);
        return PlanStage::DEAD;
    }

    if ((0 != _params.maxScan) && (_specificStats.docsTested >= _params.maxScan)) {
        _commonStats.isEOF = true;
    }

    if (_commonStats.isEOF) {
        return PlanStage::IS_EOF;
    }

    boost::optional<Record> record;
    const bool needToMakeCursor = !_cursor;
    try {
        if (needToMakeCursor) {
            const bool forward = _params.direction == CollectionScanParams::FORWARD;

            if (forward && !_params.tailable && _params.collection->ns().isOplog()) {
                // Forward, non-tailable scans from the oplog need to wait until all oplog entries
                // before the read begins to be visible. This isn't needed for reverse scans because
                // we only hide oplog entries from forward scans, and it isn't necessary for tailing
                // cursors because they ignore EOF and will eventually see all writes. Forward,
                // non-tailable scans are the only case where a meaningful EOF will be seen that
                // might not include writes that finished before the read started. This also must be
                // done before we create the cursor as that is when we establish the endpoint for
                // the cursor. Also call abandonSnapshot to make sure that we are using a fresh
                // storage engine snapshot while waiting. Otherwise, we will end up reading from
                // the snapshot where the oplog entries are not yet visible even after the wait.
                getOpCtx()->recoveryUnit()->abandonSnapshot();
                _params.collection->getRecordStore()->waitForAllEarlierOplogWritesToBeVisible(
                    getOpCtx());
            }

			//初始化CollectionScan的游标_cursor成员变量  Collection的getCursor函数得到的游标
            _cursor = _params.collection->getCursor(getOpCtx(), forward);

            if (!_lastSeenId.isNull()) {
                invariant(_params.tailable);
                // Seek to where we were last time. If it no longer exists, mark us as dead
                // since we want to signal an error rather than silently dropping data from the
                // stream. This is related to the _lastSeenId handling in invalidate. Note that
                // we want to return the record *after* this one since we have already returned
                // this one. This is only possible in the tailing case because that is the only
                // time we'd need to create a cursor after already getting a record out of it.
                //调用_cursor的seekExact函数得到一条记录,将记录的_id字段记录到_lastSeenId成员变量(下面的_lastSeenId = record->id;),
                //以便下次从这个记录之后取值,对于wiredtiger引擎直接调用_cursor->next()获取下一个值.
                if (!_cursor->seekExact(_lastSeenId)) {//WiredTigerRecordStoreCursorBase::seekExact //后去一行记录
                    _isDead = true;
                    Status status(ErrorCodes::CappedPositionLost,
                                  str::stream() << "CollectionScan died due to failure to restore "
                                                << "tailable cursor position. "
                                                << "Last seen record id: "
                                                << _lastSeenId);
                    *out = WorkingSetCommon::allocateStatusMember(_workingSet, status);
                    return PlanStage::DEAD;
                }
            }

            return PlanStage::NEED_TIME;
        }

        if (_lastSeenId.isNull() && !_params.start.isNull()) {
			//后去一行记录
            record = _cursor->seekExact(_params.start);//WiredTigerRecordStoreCursorBase::seekExact
        } else {
            // See if the record we're about to access is in memory. If not, pass a fetch
            // request up.
            if (auto fetcher = _cursor->fetcherForNext()) { 
                // Pass the RecordFetcher up.
                WorkingSetMember* member = _workingSet->get(_wsidForFetch);
                member->setFetcher(fetcher.release());
                *out = _wsidForFetch;
                return PlanStage::NEED_YIELD;
            }

			//WiredTigerRecordStoreCursorBase::next
            record = _cursor->next(); //获取一行记录信息
        }
    } catch (const WriteConflictException&) {
        // Leave us in a state to try again next time.
        if (needToMakeCursor)
            _cursor.reset();
        *out = WorkingSet::INVALID_ID;
        return PlanStage::NEED_YIELD;
    }

    if (!record) {
        // We just hit EOF. If we are tailable and have already returned data, leave us in a
        // state to pick up where we left off on the next call to work(). Otherwise EOF is
        // permanent.
        if (_params.tailable && !_lastSeenId.isNull()) {
            _cursor.reset();
        } else {
            _commonStats.isEOF = true;
        }

        return PlanStage::IS_EOF;
    }

    _lastSeenId = record->id;
    if (_params.shouldTrackLatestOplogTimestamp) {
        auto status = setLatestOplogEntryTimestamp(*record);
        if (!status.isOK()) {
            *out = WorkingSetCommon::allocateStatusMember(_workingSet, status);
            return PlanStage::FAILURE;
        }
    }

	//在WorkingSet集合中找到一个可用的位置来存放这条记录,WorkingSetMember的loc字段为记录的id字段,
	//obj字段记录的bson文档
    WorkingSetID id = _workingSet->allocate();
    WorkingSetMember* member = _workingSet->get(id);
    member->recordId = record->id;
	//WiredTigerRecoveryUnit::getSnapshotId
    member->obj = {getOpCtx()->recoveryUnit()->getSnapshotId(), record->data.releaseToBson()};
    _workingSet->transitionToRecordIdAndObj(id);

	//最后调用returnIfMatches,查看这条全表扫描的记录是否符合我们的CollectionScan这个PlanStage的filter.
	//如果符合则返回给PlanExecutor的getNext函数,否则继续往后遍历.
    return returnIfMatches(member, id, out); //CollectionScan::returnIfMatches
}

Status CollectionScan::setLatestOplogEntryTimestamp(const Record& record) {
    auto tsElem = record.data.toBson()[repl::OpTime::kTimestampFieldName];
    if (tsElem.type() != BSONType::bsonTimestamp) {
        Status status(ErrorCodes::InternalError,
                      str::stream() << "CollectionScan was asked to track latest operation time, "
                                       "but found a result without a valid 'ts' field: "
                                    << record.data.toBson().toString());
        return status;
    }
    _latestOplogEntryTimestamp = std::max(_latestOplogEntryTimestamp, tsElem.timestamp());
    return Status::OK();
}

//查看这条全表扫描的记录是否符合我们的CollectionScan这个PlanStage的filter.
//如果符合则返回给PlanExecutor的getNext函数,否则继续往后遍历.
PlanStage::StageState CollectionScan::returnIfMatches(WorkingSetMember* member,
                                                      WorkingSetID memberID,
                                                      WorkingSetID* out) {
    ++_specificStats.docsTested;

    if (Filter::passes(member, _filter)) {
        if (_params.stopApplyingFilterAfterFirstMatch) {
            _filter = nullptr;
        }
        *out = memberID;
        return PlanStage::ADVANCED;
    } else if (_endCondition && Filter::passes(member, _endCondition.get())) {
        _workingSet->free(memberID);
        _commonStats.isEOF = true;
        return PlanStage::IS_EOF;
    } else { //查询结果不满足要求
        _workingSet->free(memberID);
        return PlanStage::NEED_TIME;
    }
}

bool CollectionScan::isEOF() {
    return _commonStats.isEOF || _isDead;
}

void CollectionScan::doInvalidate(OperationContext* opCtx,
                                  const RecordId& id,
                                  InvalidationType type) {
    // We don't care about mutations since we apply any filters to the result when we (possibly)
    // return it.
    if (INVALIDATION_DELETION != type) {
        return;
    }

    // If we're here, 'id' is being deleted.

    // Deletions can harm the underlying RecordCursor so we must pass them down.
    if (_cursor) {
        _cursor->invalidate(opCtx, id);
    }

    if (_params.tailable && id == _lastSeenId) {
        // This means that deletes have caught up to the reader. We want to error in this case
        // so readers don't miss potentially important data.
        _isDead = true;
    }
}

void CollectionScan::doSaveState() {
    if (_cursor) {
        _cursor->save();
    }
}

void CollectionScan::doRestoreState() {
    if (_cursor) {
        if (!_cursor->restore()) {
            _isDead = true;
        }
    }
}

void CollectionScan::doDetachFromOperationContext() {
    if (_cursor)
        _cursor->detachFromOperationContext();
}

void CollectionScan::doReattachToOperationContext() {
    if (_cursor)
        _cursor->reattachToOperationContext(getOpCtx());
}

unique_ptr<PlanStageStats> CollectionScan::getStats() {
    // Add a BSON representation of the filter to the stats tree, if there is one.
    if (NULL != _filter) {
        BSONObjBuilder bob;
        _filter->serialize(&bob);
        _commonStats.filter = bob.obj();
    }

    unique_ptr<PlanStageStats> ret = make_unique<PlanStageStats>(_commonStats, STAGE_COLLSCAN);
    ret->specific = make_unique<CollectionScanStats>(_specificStats);
    return ret;
}

const SpecificStats* CollectionScan::getSpecificStats() const {
    return &_specificStats;
}

}  // namespace mongo
