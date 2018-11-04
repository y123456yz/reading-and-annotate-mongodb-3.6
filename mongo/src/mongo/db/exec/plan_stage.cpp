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

//#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kQuery

#include "mongo/platform/basic.h"

#include "mongo/db/exec/plan_stage.h"

#include "mongo/db/exec/scoped_timer.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/service_context.h"

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kQuery
#include "mongo/util/log.h"


namespace mongo {
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

	(gdb) bt
#0  mongo::CollectionScan::doWork (this=0x7ffa9a401140, out=0x7ffa913668d0) at src/mongo/db/exec/collection_scan.cpp:82
#1  0x00007ffa9263064b in mongo::PlanStage::work (this=0x7ffa9a401140, out=out@entry=0x7ffa913668d0) at src/mongo/db/exec/plan_stage.cpp:73
#2  0x00007ffa923059da in mongo::PlanExecutor::getNextImpl (this=0x7ffa9a403e00, objOut=objOut@entry=0x7ffa913669d0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:611
#3  0x00007ffa923064eb in mongo::PlanExecutor::getNext (this=<optimized out>, objOut=objOut@entry=0x7ffa91366af0, dlOut=dlOut@entry=0x0) at src/mongo/db/query/plan_executor.cpp:440
#4  0x00007ffa91f6ac55 in mongo::(anonymous namespace)::FindCmd::run (this=this@entry=0x7ffa94247740 <mongo::(anonymous namespace)::findCmd>, opCtx=opCtx@entry=0x7ffa9a401640, dbname=..., cmdObj=..., result=...)
		at src/mongo/db/commands/find_cmd.cpp:370
	*/ //PlanStage可以参考https://yq.aliyun.com/articles/215016?spm=a2c4e.11155435.0.0.21ad5df01WAL0E
PlanStage::StageState PlanStage::work(WorkingSetID* out) {  
    invariant(_opCtx);
    ScopedTimer timer(getClock(), &_commonStats.executionTimeMillis);
    ++_commonStats.works;

	log() << "yang test PlanStage::work";
    StageState workResult = doWork(out); //IndexScan::doWork(走索引)  CollectionScan::doWork(全表扫描)

    if (StageState::ADVANCED == workResult) {
        ++_commonStats.advanced;
    } else if (StageState::NEED_TIME == workResult) {
        ++_commonStats.needTime;
    } else if (StageState::NEED_YIELD == workResult) {
        ++_commonStats.needYield;
    }

    return workResult;
}

void PlanStage::saveState() {
    ++_commonStats.yields;
    for (auto&& child : _children) {
        child->saveState();
    }

    doSaveState();
}

void PlanStage::restoreState() {
    ++_commonStats.unyields;
    for (auto&& child : _children) {
        child->restoreState();
    }

    doRestoreState();
}

void PlanStage::invalidate(OperationContext* opCtx, const RecordId& dl, InvalidationType type) {
    ++_commonStats.invalidates;
    for (auto&& child : _children) {
        child->invalidate(opCtx, dl, type);
    }

    doInvalidate(opCtx, dl, type);
}

void PlanStage::detachFromOperationContext() {
    invariant(_opCtx);
    _opCtx = nullptr;

    for (auto&& child : _children) {
        child->detachFromOperationContext();
    }

    doDetachFromOperationContext();
}

void PlanStage::reattachToOperationContext(OperationContext* opCtx) {
    invariant(_opCtx == nullptr);
    _opCtx = opCtx;

    for (auto&& child : _children) {
        child->reattachToOperationContext(opCtx);
    }

    doReattachToOperationContext();
}

ClockSource* PlanStage::getClock() const {
    return _opCtx->getServiceContext()->getFastClockSource();
}

}  // namespace mongo
