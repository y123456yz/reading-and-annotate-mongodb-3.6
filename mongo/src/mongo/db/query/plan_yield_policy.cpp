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

#include "mongo/platform/basic.h"

#include "mongo/db/query/plan_yield_policy.h"

#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/curop.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/query/query_knobs.h"
#include "mongo/db/query/query_yield.h"
#include "mongo/db/service_context.h"
#include "mongo/util/scopeguard.h"
#include "mongo/util/time_support.h"

namespace mongo {
/*
Mongodb使用WiredTiger提供的SnapshotIsolation 隔离级别。但不代表Mongodb的查询是该隔离级别。
Mongodb的查询过程中，会阶段性的将过程Yield出去，一方面是为了检测过程是否已经被终止，一方面
是为了让出时间片给其它线程执行。而Yield出去的查询，会连带释放掉WiredTiger层的Snapshot。因此，
Mongodb的查询操作的事务隔离级别是Read-Committed隔离级别的。


mongodb 在执行一个耗时较长的查询时，可以通过db.killOp()命令结束。 它是通过YieldPolicy做到这点的。
具体到查询而言，查询使用YieldAuto Policy。所谓YieldAuto，是指查询请求会运行一段时间（可配置）后
让出CPU时间片，并检测自己是否被killOp命令kill掉。这是一种典型的协作式调度策略。
*/
PlanYieldPolicy::PlanYieldPolicy(PlanExecutor* exec, PlanExecutor::YieldPolicy policy)
    : _policy(exec->getOpCtx()->lockState()->isGlobalLockedRecursively() ? PlanExecutor::NO_YIELD
                                                                         : policy),
      _forceYield(false),
      _elapsedTracker(exec->getOpCtx()->getServiceContext()->getFastClockSource(),
                      internalQueryExecYieldIterations.load(),
                      Milliseconds(internalQueryExecYieldPeriodMS.load())),
      _planYielding(exec) {}


PlanYieldPolicy::PlanYieldPolicy(PlanExecutor::YieldPolicy policy, ClockSource* cs)
    : _policy(policy),
      _forceYield(false),
      _elapsedTracker(cs,
                      internalQueryExecYieldIterations.load(),
                      Milliseconds(internalQueryExecYieldPeriodMS.load())),
      _planYielding(nullptr) {}

bool PlanYieldPolicy::shouldYield() {
    if (!canAutoYield())
        return false;
    invariant(!_planYielding->getOpCtx()->lockState()->inAWriteUnitOfWork());
    if (_forceYield)
        return true;
    return _elapsedTracker.intervalHasElapsed();
}

void PlanYieldPolicy::resetTimer() {
    _elapsedTracker.resetLastTime();
}

Status PlanYieldPolicy::yield(RecordFetcher* recordFetcher) {
    invariant(_planYielding);
    if (recordFetcher) {
        OperationContext* opCtx = _planYielding->getOpCtx();
        return yield([recordFetcher, opCtx] { recordFetcher->setup(opCtx); },
                     [recordFetcher] { recordFetcher->fetch(); });
    } else {
        return yield(nullptr, nullptr);
    }
}

Status PlanYieldPolicy::yield(stdx::function<void()> beforeYieldingFn,
                              stdx::function<void()> whileYieldingFn) {
    invariant(_planYielding);
    invariant(canAutoYield());

    // After we finish yielding (or in any early return), call resetTimer() to prevent yielding
    // again right away. We delay the resetTimer() call so that the clock doesn't start ticking
    // until after we return from the yield.
    ON_BLOCK_EXIT([this]() { resetTimer(); });

    _forceYield = false;

    OperationContext* opCtx = _planYielding->getOpCtx();
    invariant(opCtx);
    invariant(!opCtx->lockState()->inAWriteUnitOfWork());

    // Can't use writeConflictRetry since we need to call saveState before reseting the transaction.
    for (int attempt = 1; true; attempt++) {
        try {
            // All YIELD_AUTO plans will get here eventually when the elapsed tracker triggers
            // that it's time to yield. Whether or not we will actually yield, we need to check
            // if this operation has been interrupted.
            if (_policy == PlanExecutor::YIELD_AUTO) { // 检查是否被kill掉了 
                auto interruptStatus = opCtx->checkForInterruptNoAssert();
                if (!interruptStatus.isOK()) {
                    return interruptStatus;
                }
            }

            try {
                _planYielding->saveState();
            } catch (const WriteConflictException&) {
                invariant(!"WriteConflictException not allowed in saveState");
            }

            if (_policy == PlanExecutor::WRITE_CONFLICT_RETRY_ONLY) {
                // Just reset the snapshot. Leave all LockManager locks alone.
                opCtx->recoveryUnit()->abandonSnapshot();
            } else {
                // Release and reacquire locks.
                if (beforeYieldingFn)
                    beforeYieldingFn();
                QueryYield::yieldAllLocks(opCtx, whileYieldingFn, _planYielding->nss());
            }

            return _planYielding->restoreStateWithoutRetrying();
        } catch (const WriteConflictException&) {
            CurOp::get(opCtx)->debug().writeConflicts++;
            WriteConflictException::logAndBackoff(
                attempt, "plan execution restoreState", _planYielding->nss().ns());
            // retry
        }
    }
}

}  // namespace mongo
