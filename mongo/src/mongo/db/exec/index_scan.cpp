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

#include "mongo/platform/basic.h"

#include "mongo/db/exec/index_scan.h"

#include "mongo/db/catalog/index_catalog.h"
#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/exec/filter.h"
#include "mongo/db/exec/scoped_timer.h"
#include "mongo/db/exec/working_set_computed_data.h"
#include "mongo/db/index/index_access_method.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/query/index_bounds_builder.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"

namespace {

// Return a value in the set {-1, 0, 1} to represent the sign of parameter i.
int sgn(int i) {
    if (i == 0)
        return 0;
    return i > 0 ? 1 : -1;
}

}  // namespace

namespace mongo {

// static
const char* IndexScan::kStageType = "IXSCAN";
//buildStages中构造使用
IndexScan::IndexScan(OperationContext* opCtx,
                     const IndexScanParams& params,
                     WorkingSet* workingSet,
                     const MatchExpression* filter)
    : PlanStage(kStageType, opCtx),
      _workingSet(workingSet),
      _iam(params.descriptor->getIndexCatalog()->getIndex(params.descriptor)),
      _keyPattern(params.descriptor->keyPattern().getOwned()),
      _scanState(INITIALIZING),
      _filter(filter),
      _shouldDedup(true),
      _forward(params.direction == 1),
      _params(params),
      _startKeyInclusive(IndexBounds::isStartIncludedInBound(params.bounds.boundInclusion)),
      _endKeyInclusive(IndexBounds::isEndIncludedInBound(params.bounds.boundInclusion)) {
    // We can't always access the descriptor in the call to getStats() so we pull
    // any info we need for stats reporting out here.
    _specificStats.keyPattern = _keyPattern;
    if (BSONElement collationElement = _params.descriptor->getInfoElement("collation")) {
        invariant(collationElement.isABSONObj());
        _specificStats.collation = collationElement.Obj().getOwned();
    }
    _specificStats.indexName = _params.descriptor->indexName();
    _specificStats.isMultiKey = _params.descriptor->isMultikey(getOpCtx());
    _specificStats.multiKeyPaths = _params.descriptor->getMultikeyPaths(getOpCtx());
    _specificStats.isUnique = _params.descriptor->unique();
    _specificStats.isSparse = _params.descriptor->isSparse();
    _specificStats.isPartial = _params.descriptor->isPartial();
    _specificStats.indexVersion = static_cast<int>(_params.descriptor->version());
}

/*
(gdb) bt
#0  mongo::IndexScan::initIndexScan (this=this@entry=0x7f88328f8000) at src/mongo/db/exec/index_scan.cpp:102
#1  0x00007f882ae8172f in mongo::IndexScan::doWork (this=0x7f88328f8000, out=0x7f8829bcb918) at src/mongo/db/exec/index_scan.cpp:138
#2  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f88328f8000, out=out@entry=0x7f8829bcb918) at src/mongo/db/exec/plan_stage.cpp:46
#3  0x00007f882ae70855 in mongo::FetchStage::doWork (this=0x7f8832110500, out=0x7f8829bcb9e0) at src/mongo/db/exec/fetch.cpp:86
#4  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832110500, out=out@entry=0x7f8829bcb9e0) at src/mongo/db/exec/plan_stage.cpp:46
#5  0x00007f882ab6a823 in mongo::PlanExecutor::getNextImpl (this=0x7f8832362000, objOut=objOut@entry=0x7f8829bcba70, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:546
#6  0x00007f882ab6b16b in mongo::PlanExecutor::getNext (this=<optimized out>, objOut=objOut@entry=0x7f8829bcbb80, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:406
#7  0x00007f882a7cfc3d in mongo::(anonymous namespace)::FindCmd::run (this=this@entry=0x7f882caac740 <mongo::(anonymous namespace)::findCmd>, opCtx=opCtx@entry=0x7f883216fdc0, dbname=..., cmdObj=..., result=...)
    at src/mongo/db/commands/find_cmd.cpp:366

(gdb) bt
#0  mongo::IndexScan::initIndexScan (this=this@entry=0x7f8832913800) at src/mongo/db/exec/index_scan.cpp:102
#1  0x00007f882ae8172f in mongo::IndexScan::doWork (this=0x7f8832913800, out=0x7f8820d0dc18) at src/mongo/db/exec/index_scan.cpp:138
#2  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832913800, out=out@entry=0x7f8820d0dc18) at src/mongo/db/exec/plan_stage.cpp:46
#3  0x00007f882ae70855 in mongo::FetchStage::doWork (this=0x7f8832110880, out=0x7f8820d0dcf8) at src/mongo/db/exec/fetch.cpp:86
#4  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832110880, out=out@entry=0x7f8820d0dcf8) at src/mongo/db/exec/plan_stage.cpp:46
#5  0x00007f882ae6c318 in mongo::DeleteStage::doWork (this=0x7f8832363400, out=0x7f8820d0de40) at src/mongo/db/exec/delete.cpp:125
#6  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832363400, out=out@entry=0x7f8820d0de40) at src/mongo/db/exec/plan_stage.cpp:46
#7  0x00007f882ab6a823 in mongo::PlanExecutor::getNextImpl (this=this@entry=0x7f8832363500, objOut=objOut@entry=0x7f8820d0ded0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:546
#8  0x00007f882ab6b16b in mongo::PlanExecutor::getNext (this=this@entry=0x7f8832363500, objOut=objOut@entry=0x7f8820d0df20, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:406
#9  0x00007f882ab6b26d in mongo::PlanExecutor::executePlan (this=0x7f8832363500) at src/mongo/db/query/plan_executor.cpp:665
#10 0x00007f882a76e92c in mongo::TTLMonitor::doTTLForIndex (this=this@entry=0x7f882e8cdfc0, opCtx=opCtx@entry=0x7f8832170180, idx=...) at src/mongo/db/ttl.cpp:263
#11 0x00007f882a76f5e0 in mongo::TTLMonitor::doTTLPass (this=this@entry=0x7f882e8cdfc0) at src/mongo/db/ttl.cpp:158
#12 0x00007f882a76fc08 in mongo::TTLMonitor::run (this=0x7f882e8cdfc0) at src/mongo/db/ttl.cpp:111
#13 0x00007f882bc3b221 in mongo::BackgroundJob::jobBody (this=0x7f882e8cdfc0) at src/mongo/util/background.cpp:150
*/
boost::optional<IndexKeyEntry> IndexScan::initIndexScan() {
    if (_params.doNotDedup) {
        _shouldDedup = false;
    } else {
        // TODO it is incorrect to rely on this not changing. SERVER-17678
        _shouldDedup = _params.descriptor->isMultikey(getOpCtx());
    }

    // Perform the possibly heavy-duty initialization of the underlying index cursor.
    //WiredTigerIndexUniqueCursor结构
    _indexCursor = _iam->newCursor(getOpCtx(), _forward); //WiredTigerIndexUnique::newCursor   

    // We always seek once to establish the cursor position.
    ++_specificStats.seeks;

	//也就是db.test.find({"name": "yangyazhou"}).explain("allPlansExecution")返回中的indexBounds内容，指定key范围
    if (_params.bounds.isSimpleRange) {
        // Start at one key, end at another.
        _startKey = _params.bounds.startKey;
        _endKey = _params.bounds.endKey;
		//WiredTigerIndexCursorBase::setEndPosition
        _indexCursor->setEndPosition(_endKey, _endKeyInclusive);
		//WiredTigerIndexCursorBase::seek
        return _indexCursor->seek(_startKey, _startKeyInclusive);
    } else {
        // For single intervals, we can use an optimized scan which checks against the position
        // of an end cursor.  For all other index scans, we fall back on using
        // IndexBoundsChecker to determine when we've finished the scan.
        if (IndexBoundsBuilder::isSingleInterval(
                _params.bounds, &_startKey, &_startKeyInclusive, &_endKey, &_endKeyInclusive)) {
            _indexCursor->setEndPosition(_endKey, _endKeyInclusive); //WiredTigerIndexCursorBase::setEndPosition
            return _indexCursor->seek(_startKey, _startKeyInclusive); //WiredTigerIndexCursorBase::seek
        } else {
            _checker.reset(new IndexBoundsChecker(&_params.bounds, _keyPattern, _params.direction));

            if (!_checker->getStartSeekPoint(&_seekPoint))
                return boost::none;
			//WiredTigerIndexCursorBase::seek
            return _indexCursor->seek(_seekPoint);
        }
    }
}
/*
(gdb) bt
#0  mongo::IndexScan::initIndexScan (this=this@entry=0x7f88328f8000) at src/mongo/db/exec/index_scan.cpp:102
#1  0x00007f882ae8172f in mongo::IndexScan::doWork (this=0x7f88328f8000, out=0x7f8829bcb918) at src/mongo/db/exec/index_scan.cpp:138
#2  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f88328f8000, out=out@entry=0x7f8829bcb918) at src/mongo/db/exec/plan_stage.cpp:46
#3  0x00007f882ae70855 in mongo::FetchStage::doWork (this=0x7f8832110500, out=0x7f8829bcb9e0) at src/mongo/db/exec/fetch.cpp:86
#4  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832110500, out=out@entry=0x7f8829bcb9e0) at src/mongo/db/exec/plan_stage.cpp:46
#5  0x00007f882ab6a823 in mongo::PlanExecutor::getNextImpl (this=0x7f8832362000, objOut=objOut@entry=0x7f8829bcba70, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:546
#6  0x00007f882ab6b16b in mongo::PlanExecutor::getNext (this=<optimized out>, objOut=objOut@entry=0x7f8829bcbb80, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:406
#7  0x00007f882a7cfc3d in mongo::(anonymous namespace)::FindCmd::run (this=this@entry=0x7f882caac740 <mongo::(anonymous namespace)::findCmd>, opCtx=opCtx@entry=0x7f883216fdc0, dbname=..., cmdObj=..., result=...)
    at src/mongo/db/commands/find_cmd.cpp:366

(gdb) bt
#0  mongo::IndexScan::initIndexScan (this=this@entry=0x7f8832913800) at src/mongo/db/exec/index_scan.cpp:102
#1  0x00007f882ae8172f in mongo::IndexScan::doWork (this=0x7f8832913800, out=0x7f8820d0dc18) at src/mongo/db/exec/index_scan.cpp:138
#2  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832913800, out=out@entry=0x7f8820d0dc18) at src/mongo/db/exec/plan_stage.cpp:46
#3  0x00007f882ae70855 in mongo::FetchStage::doWork (this=0x7f8832110880, out=0x7f8820d0dcf8) at src/mongo/db/exec/fetch.cpp:86
#4  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832110880, out=out@entry=0x7f8820d0dcf8) at src/mongo/db/exec/plan_stage.cpp:46
#5  0x00007f882ae6c318 in mongo::DeleteStage::doWork (this=0x7f8832363400, out=0x7f8820d0de40) at src/mongo/db/exec/delete.cpp:125
#6  0x00007f882ae952cb in mongo::PlanStage::work (this=0x7f8832363400, out=out@entry=0x7f8820d0de40) at src/mongo/db/exec/plan_stage.cpp:46
#7  0x00007f882ab6a823 in mongo::PlanExecutor::getNextImpl (this=this@entry=0x7f8832363500, objOut=objOut@entry=0x7f8820d0ded0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:546
#8  0x00007f882ab6b16b in mongo::PlanExecutor::getNext (this=this@entry=0x7f8832363500, objOut=objOut@entry=0x7f8820d0df20, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:406
#9  0x00007f882ab6b26d in mongo::PlanExecutor::executePlan (this=0x7f8832363500) at src/mongo/db/query/plan_executor.cpp:665
#10 0x00007f882a76e92c in mongo::TTLMonitor::doTTLForIndex (this=this@entry=0x7f882e8cdfc0, opCtx=opCtx@entry=0x7f8832170180, idx=...) at src/mongo/db/ttl.cpp:263
#11 0x00007f882a76f5e0 in mongo::TTLMonitor::doTTLPass (this=this@entry=0x7f882e8cdfc0) at src/mongo/db/ttl.cpp:158
#12 0x00007f882a76fc08 in mongo::TTLMonitor::run (this=0x7f882e8cdfc0) at src/mongo/db/ttl.cpp:111
#13 0x00007f882bc3b221 in mongo::BackgroundJob::jobBody (this=0x7f882e8cdfc0) at src/mongo/util/background.cpp:150
*/
//IndexScan::doWork(走索引)  CollectionScan::doWork(全表扫描)

//IndexScan::doWork走索引的情况下，每获取到一个index的key-value(value实际上是数据文件的key，也就指定了数据文件位置)(获取满足key的索引数据)，
//该函数返回后会通过PlanStage::work调用FetchStage::doWork走fetch流程来对IndexScan::doWork获取到的索引key-value中的value来获取对应的数据
PlanStage::StageState IndexScan::doWork(WorkingSetID* out) { //PlanStage::work中执行
    // Get the next kv pair from the index, if any.
    boost::optional<IndexKeyEntry> kv; //索引文件key，及其在数据文件中的位置
    try {
        switch (_scanState) {
            case INITIALIZING:
                kv = initIndexScan();
                break;
            case GETTING_NEXT:
                kv = _indexCursor->next(); //WiredTigerIndexCursorBase::next  如果没有传参，默认kKeyAndLoc，见 SortedDataInterface::Cursor
                break;
            case NEED_SEEK:
                ++_specificStats.seeks;
                kv = _indexCursor->seek(_seekPoint);
                break;
            case HIT_END:
                return PlanStage::IS_EOF;
        }
    } catch (const WriteConflictException&) {
        *out = WorkingSet::INVALID_ID;
        return PlanStage::NEED_YIELD;
    }

    if (kv) {
        // In debug mode, check that the cursor isn't lying to us.
        if (kDebugBuild && !_startKey.isEmpty()) {
            int cmp = kv->key.woCompare(_startKey,
                                        Ordering::make(_params.descriptor->keyPattern()),
                                        /*compareFieldNames*/ false);
            if (cmp == 0)
                dassert(_startKeyInclusive);
            dassert(_forward ? cmp >= 0 : cmp <= 0);
        }

        if (kDebugBuild && !_endKey.isEmpty()) {
            int cmp = kv->key.woCompare(_endKey,
                                        Ordering::make(_params.descriptor->keyPattern()),
                                        /*compareFieldNames*/ false);
            if (cmp == 0)
                dassert(_endKeyInclusive);
            dassert(_forward ? cmp <= 0 : cmp >= 0);
        }

		//size_t docsExamined; FetchStage::returnIfMatches中自增     keysExamined在IndexScan::doWork自增
        ++_specificStats.keysExamined;
        if (_params.maxScan && _specificStats.keysExamined >= _params.maxScan) { //查询的时候指定了最多扫描maxScan条
            kv = boost::none;
        }
    }

    if (kv && _checker) {
		//
        switch (_checker->checkKey(kv->key, &_seekPoint)) {
            case IndexBoundsChecker::VALID: 
                break;

            case IndexBoundsChecker::DONE:
                kv = boost::none;
                break;

            case IndexBoundsChecker::MUST_ADVANCE:
                _scanState = NEED_SEEK;
                return PlanStage::NEED_TIME;
        }
    }

    if (!kv) {
        _scanState = HIT_END;
        _commonStats.isEOF = true;
        _indexCursor.reset();
        return PlanStage::IS_EOF;
    }

    _scanState = GETTING_NEXT;

    if (_shouldDedup) {
        ++_specificStats.dupsTested;
        if (!_returned.insert(kv->loc).second) {
            // We've seen this RecordId before. Skip it this time.
            ++_specificStats.dupsDropped;
            return PlanStage::NEED_TIME;
        }
    }

    if (_filter) { //filter : 查询过滤条件，类比SQL的where表达式
        if (!Filter::passes(kv->key, _keyPattern, _filter)) {
            return PlanStage::NEED_TIME;
        }
    }

    if (!kv->key.isOwned())
        kv->key = kv->key.getOwned();

    // We found something to return, so fill out the WSM.
    //一个WorkingSetID对应一个WorkingSetMember
    WorkingSetID id = _workingSet->allocate(); //WorkingSet::allocate
    WorkingSetMember* member = _workingSet->get(id);//WorkingSet::get
    member->recordId = kv->loc; //根据在数据文件中的位置，获取到数据文件中的kv
    member->keyData.push_back(IndexKeyDatum(_keyPattern, kv->key, _iam));
    _workingSet->transitionToRecordIdAndIdx(id);

    if (_params.addKeyMetadata) {
        BSONObjBuilder bob;
        bob.appendKeys(_keyPattern, kv->key);
        member->addComputed(new IndexKeyComputedData(bob.obj()));
    }

    *out = id; //这里面有索引信息 
    return PlanStage::ADVANCED;
}

bool IndexScan::isEOF() {
    return _commonStats.isEOF;
}

void IndexScan::doSaveState() {
    if (!_indexCursor)
        return;

    if (_scanState == NEED_SEEK) {
        _indexCursor->saveUnpositioned();
        return;
    }

    _indexCursor->save();
}

void IndexScan::doRestoreState() {
    if (_indexCursor)
        _indexCursor->restore();
}

void IndexScan::doDetachFromOperationContext() {
    if (_indexCursor)
        _indexCursor->detachFromOperationContext();
}

void IndexScan::doReattachToOperationContext() {
    if (_indexCursor)
        _indexCursor->reattachToOperationContext(getOpCtx());
}

void IndexScan::doInvalidate(OperationContext* opCtx, const RecordId& dl, InvalidationType type) {
    // The only state we're responsible for holding is what RecordIds to drop.  If a document
    // mutates the underlying index cursor will deal with it.
    if (INVALIDATION_MUTATION == type) {
        return;
    }

    // If we see this RecordId again, it may not be the same document it was before, so we want
    // to return it if we see it again.
    unordered_set<RecordId, RecordId::Hasher>::iterator it = _returned.find(dl);
    if (it != _returned.end()) {
        ++_specificStats.seenInvalidated;
        _returned.erase(it);
    }
}

std::unique_ptr<PlanStageStats> IndexScan::getStats() {
    // WARNING: this could be called even if the collection was dropped.  Do not access any
    // catalog information here.

    // Add a BSON representation of the filter to the stats tree, if there is one.
    if (NULL != _filter) {
        BSONObjBuilder bob;
        _filter->serialize(&bob);
        _commonStats.filter = bob.obj();
    }

    // These specific stats fields never change.
    if (_specificStats.indexType.empty()) {
        _specificStats.indexType = "BtreeCursor";  // TODO amName;

        _specificStats.indexBounds = _params.bounds.toBSON();

        _specificStats.direction = _params.direction;
    }

    std::unique_ptr<PlanStageStats> ret =
        stdx::make_unique<PlanStageStats>(_commonStats, STAGE_IXSCAN);
    ret->specific = stdx::make_unique<IndexScanStats>(_specificStats);
    return ret;
}

const SpecificStats* IndexScan::getSpecificStats() const {
    return &_specificStats;
}

}  // namespace mongo
