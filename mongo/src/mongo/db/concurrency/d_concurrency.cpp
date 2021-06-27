/**
 *    Copyright (C) 2008-2014 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kDefault

#include "mongo/platform/basic.h"

#include "mongo/db/concurrency/d_concurrency.h"

#include <string>
#include <vector>

#include "mongo/db/concurrency/global_lock_acquisition_tracker.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/server_parameters.h"
#include "mongo/db/service_context.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/mutex.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/stacktrace.h"

namespace mongo {

Lock::TempRelease::TempRelease(Locker* lockState)
    : _lockState(lockState),
      _lockSnapshot(),
      _locksReleased(_lockState->saveLockStateAndUnlock(&_lockSnapshot)) {}

Lock::TempRelease::~TempRelease() {
    if (_locksReleased) {
        invariant(!_lockState->isLocked());
        _lockState->restoreLockState(_lockSnapshot);
    }
}

namespace {

/**
 * ResourceMutexes can be constructed during initialization, thus the code must ensure the vector
 * of labels is constructed before items are added to it. This factory encapsulates all members
 * that need to be initialized before first use. A pointer is allocated to an instance of this
 * factory and the first call will construct an instance.
 */
class ResourceIdFactory {
public:
    static ResourceId newResourceIdForMutex(std::string resourceLabel) {
        ensureInitialized();
        return resourceIdFactory->_newResourceIdForMutex(std::move(resourceLabel));
    }

    static std::string nameForId(ResourceId resourceId) {
        stdx::lock_guard<stdx::mutex> lk(resourceIdFactory->labelsMutex);
        return resourceIdFactory->labels.at(resourceId.getHashId());
    }

    /**
     * Must be called in a single-threaded context (e.g: program initialization) before the factory
     * is safe to use in a multi-threaded context.
     */
    static void ensureInitialized() {
        if (!resourceIdFactory) {
            resourceIdFactory = new ResourceIdFactory();
        }
    }

private:
	//生成RESOURCE_MUTEX类型的ResourceId
    ResourceId _newResourceIdForMutex(std::string resourceLabel) {
        stdx::lock_guard<stdx::mutex> lk(labelsMutex);
        invariant(nextId == labels.size());
        labels.push_back(std::move(resourceLabel));

        return ResourceId(RESOURCE_MUTEX, nextId++);
    }

    static ResourceIdFactory* resourceIdFactory;

    std::uint64_t nextId = 0;
    std::vector<std::string> labels;
    stdx::mutex labelsMutex;
};

ResourceIdFactory* ResourceIdFactory::resourceIdFactory;

/**
 * Guarantees `ResourceIdFactory::ensureInitialized` is called at least once during initialization.
 */
struct ResourceIdFactoryInitializer {
    ResourceIdFactoryInitializer() {
        ResourceIdFactory::ensureInitialized();
    }
} resourceIdFactoryInitializer;

}  // namespace


Lock::ResourceMutex::ResourceMutex(std::string resourceLabel)
    : _rid(ResourceIdFactory::newResourceIdForMutex(std::move(resourceLabel))) {}

std::string Lock::ResourceMutex::getName(ResourceId resourceId) {
    invariant(resourceId.getType() == RESOURCE_MUTEX);
    return ResourceIdFactory::nameForId(resourceId);
}

bool Lock::ResourceMutex::isExclusivelyLocked(Locker* locker) {
    return locker->isLockHeldForMode(_rid, MODE_X);
}

bool Lock::ResourceMutex::isAtLeastReadLocked(Locker* locker) {
    return locker->isLockHeldForMode(_rid, MODE_IS);
}
//Lock::DBLock::DBLock中构造使用，每个DBLock都有一个对应的全局锁_globalLock
Lock::GlobalLock::GlobalLock(OperationContext* opCtx, LockMode lockMode, unsigned timeoutMs)
    : GlobalLock(opCtx, lockMode, timeoutMs, EnqueueOnly()) //这里构造函数中调用Lock::GlobalLock::_enqueue获取全局锁
{
    waitForLock(timeoutMs); //这里面等待获取全局锁，直到超时或者获取到锁
}

Lock::GlobalLock::GlobalLock(OperationContext* opCtx,
                             LockMode lockMode,
                             unsigned timeoutMs,
                             EnqueueOnly enqueueOnly)
    : _opCtx(opCtx),
      _result(LOCK_INVALID),
      _pbwm(opCtx->lockState(), resourceIdParallelBatchWriterMode),
      _isOutermostLock(!opCtx->lockState()->isLocked()) {
    _enqueue(lockMode, timeoutMs);
}

Lock::GlobalLock::GlobalLock(GlobalLock&& otherLock)
    : _opCtx(otherLock._opCtx),
      _result(otherLock._result),
      _pbwm(std::move(otherLock._pbwm)),
      _isOutermostLock(otherLock._isOutermostLock) {
    // Mark as moved so the destructor doesn't invalidate the newly-constructed lock.
    otherLock._result = LOCK_INVALID;
}

//Lock::GlobalLock::GlobalLock中调用
void Lock::GlobalLock::_enqueue(LockMode lockMode, unsigned timeoutMs) {
    if (_opCtx->lockState()->shouldConflictWithSecondaryBatchApplication()) { //和同步相关
        _pbwm.lock(MODE_IS);
    }

	//LockerImpl:lockGlobalBegin->LockerImpl<>::_lockGlobalBegin
    _result = _opCtx->lockState()->lockGlobalBegin(lockMode, Milliseconds(timeoutMs));
}

//Lock::GlobalLock::GlobalLock
void Lock::GlobalLock::waitForLock(unsigned timeoutMs) {
    if (_result == LOCK_WAITING) {
		//LockerImpl<>::lockGlobalComplete 这里面等待获取全局锁，直到超时或者获取到锁
        _result = _opCtx->lockState()->lockGlobalComplete(Milliseconds(timeoutMs));
    }

    if (_result != LOCK_OK && _opCtx->lockState()->shouldConflictWithSecondaryBatchApplication()) {
        _pbwm.unlock();
    }

    if (_opCtx->lockState()->isWriteLocked()) {
        GlobalLockAcquisitionTracker::get(_opCtx).setGlobalExclusiveLockTaken();
    }
}

//解锁
void Lock::GlobalLock::_unlock() {
    if (isLocked()) {
        _opCtx->lockState()->unlockGlobal(); //LockerImpl<IsForMMAPV1>::unlockGlobal
        _result = LOCK_INVALID;
    }
}
//insertBatchAndHandleErrors->AutoGetCollection::AutoGetCollection
Lock::DBLock::DBLock(OperationContext* opCtx, StringData db, LockMode mode)
    : _id(RESOURCE_DATABASE, db),
      _opCtx(opCtx),
      _mode(mode),
      //全局锁初始化构造  Lock::GlobalLock::GlobalLock
      _globalLock(opCtx, isSharedLockMode(_mode) ? MODE_IS : MODE_IX, UINT_MAX) {
    massert(28539, "need a valid database name", !db.empty() && nsIsDbOnly(db));

    // Need to acquire the flush lock
    _opCtx->lockState()->lockMMAPV1Flush();

    // The check for the admin db is to ensure direct writes to auth collections
    // are serialized (see SERVER-16092).
    if ((_id == resourceIdAdminDB) && !isSharedLockMode(_mode)) {
        _mode = MODE_X;
    }

	//LockerImpl<>::lock
    invariant(LOCK_OK == _opCtx->lockState()->lock(_id, _mode)); //OperationContext::lockState->LockerImpl<>::lock
}

Lock::DBLock::DBLock(DBLock&& otherLock)
    : _id(otherLock._id),
      _opCtx(otherLock._opCtx),
      _mode(otherLock._mode),
      _globalLock(std::move(otherLock._globalLock)) {
    // Mark as moved so the destructor doesn't invalidate the newly-constructed lock.
    otherLock._mode = MODE_NONE;
}

/*
#0  mongo::WiredTigerRecoveryUnit::_txnClose (this=this@entry=0x7fb1db1afd80, commit=commit@entry=false) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:199
#1  0x00007fb1d44455bf in mongo::WiredTigerRecoveryUnit::abandonSnapshot (this=0x7fb1db1afd80) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:177
#2  0x00007fb1d5a71767 in ~GlobalLock (this=0x7fb1d3a5b100, __in_chrg=<optimized out>) at src/mongo/db/concurrency/d_concurrency.h:203
#3  mongo::Lock::DBLock::~DBLock (this=0x7fb1d3a5b0c8, __in_chrg=<optimized out>) at src/mongo/db/concurrency/d_concurrency.cpp:221
#4  0x00007fb1d465e659 in mongo::(anonymous namespace)::FindCmd::run
*/
Lock::DBLock::~DBLock() {
    if (_mode != MODE_NONE) {
		//OperationContext::lockState->LockerImpl<>::unlock
        _opCtx->lockState()->unlock(_id); //~GlobalLock
    }
}

void Lock::DBLock::relockWithMode(LockMode newMode) {
    // 2PL would delay the unlocking
    invariant(!_opCtx->lockState()->inAWriteUnitOfWork());

    // Not allowed to change global intent
    invariant(!isSharedLockMode(_mode) || isSharedLockMode(newMode));

    _opCtx->lockState()->unlock(_id);
    _mode = newMode;

    invariant(LOCK_OK == _opCtx->lockState()->lock(_id, _mode));
}

//AutoGetCollection::AutoGetCollection构造
Lock::CollectionLock::CollectionLock(Locker* lockState, StringData ns, LockMode mode)
    : _id(RESOURCE_COLLECTION, ns), _lockState(lockState) {
    massert(28538, "need a non-empty collection name", nsIsFull(ns));

    dassert(_lockState->isDbLockedForMode(nsToDatabaseSubstring(ns),
                                          isSharedLockMode(mode) ? MODE_IS : MODE_IX));
    if (supportsDocLocking()) {
        _lockState->lock(_id, mode);
    } else {
        _lockState->lock(_id, isSharedLockMode(mode) ? MODE_S : MODE_X);
    }
}

Lock::CollectionLock::~CollectionLock() {
    _lockState->unlock(_id);
}

void Lock::CollectionLock::relockAsDatabaseExclusive(Lock::DBLock& dbLock) {
    _lockState->unlock(_id);

    dbLock.relockWithMode(MODE_X);

    // don't need the lock, but need something to unlock in the destructor
    _lockState->lock(_id, MODE_IX);
}

namespace {
//oplock相关的锁
stdx::mutex oplogSerialization;  // for OplogIntentWriteLock
}  // namespace

Lock::OplogIntentWriteLock::OplogIntentWriteLock(Locker* lockState)
    : _lockState(lockState), _serialized(false) {
    _lockState->lock(resourceIdOplog, MODE_IX);
}

Lock::OplogIntentWriteLock::~OplogIntentWriteLock() {
    if (_serialized) {
        oplogSerialization.unlock();
    }
    _lockState->unlock(resourceIdOplog);
}

void Lock::OplogIntentWriteLock::serializeIfNeeded() {
    if (!supportsDocLocking() && !_serialized) {
        oplogSerialization.lock();
        _serialized = true;
    }
}

//db/repl/sync_tail.cpp:    Lock::ParallelBatchWriterMode pbwm(opCtx->lockState());
Lock::ParallelBatchWriterMode::ParallelBatchWriterMode(Locker* lockState)
    : _pbwm(lockState, resourceIdParallelBatchWriterMode, MODE_X),
      _lockState(lockState),
      _orginalShouldConflict(_lockState->shouldConflictWithSecondaryBatchApplication()) {
    _lockState->setShouldConflictWithSecondaryBatchApplication(false);
}

Lock::ParallelBatchWriterMode::~ParallelBatchWriterMode() {
    _lockState->setShouldConflictWithSecondaryBatchApplication(_orginalShouldConflict);
}

void Lock::ResourceLock::lock(LockMode mode) {
    invariant(_result == LOCK_INVALID);
    _result = _locker->lock(_rid, mode);
    invariant(_result == LOCK_OK);
}

void Lock::ResourceLock::unlock() {
    if (_result == LOCK_OK) {
        _locker->unlock(_rid);
        _result = LOCK_INVALID;
    }
}

/*
Breakpoint 1, mongo::Lock::GlobalLock::_enqueue (this=this@entry=0x7f0c83f3fdc8, lockMode=lockMode@entry=mongo::MODE_IX, timeoutMs=timeoutMs@entry=4294967295) at src/mongo/db/concurrency/d_concurrency.cpp:171
171         _result = _opCtx->lockState()->lockGlobalBegin(lockMode, Milliseconds(timeoutMs));
(gdb) bt
#0  mongo::Lock::GlobalLock::_enqueue (this=this@entry=0x7f0c83f3fdc8, lockMode=lockMode@entry=mongo::MODE_IX, timeoutMs=timeoutMs@entry=4294967295) at src/mongo/db/concurrency/d_concurrency.cpp:171
#1  0x00007f0c85f57dac in mongo::Lock::GlobalLock::GlobalLock (this=0x7f0c83f3fdc8, opCtx=0x7f0c8ba3c400, lockMode=mongo::MODE_IX, timeoutMs=4294967295, enqueueOnly=...) at src/mongo/db/concurrency/d_concurrency.cpp:152
#2  0x00007f0c85f57f38 in mongo::Lock::GlobalLock::GlobalLock (this=0x7f0c83f3fdc8, opCtx=<optimized out>, lockMode=<optimized out>, timeoutMs=4294967295) at src/mongo/db/concurrency/d_concurrency.cpp:140
#3  0x00007f0c85f57fda in mongo::Lock::DBLock::DBLock (this=0x7f0c83f3fd90, opCtx=0x7f0c8ba3c400, db=..., mode=mongo::MODE_IX) at src/mongo/db/concurrency/d_concurrency.cpp:201
#4  0x00007f0c84eefd14 in mongo::AutoGetCollection::AutoGetCollection (this=0x7f0c83f400a8, opCtx=0x7f0c8ba3c400, nss=..., modeDB=<optimized out>, modeColl=mongo::MODE_IX, viewMode=mongo::AutoGetCollection::kViewsForbidden)
    at src/mongo/db/db_raii.cpp:76
#5  0x00007f0c84bca2d2 in AutoGetCollection (modeAll=mongo::MODE_IX, nss=..., opCtx=0x7f0c8ba3c400, this=0x7f0c83f400a8) at src/mongo/db/db_raii.h:92
#6  emplace_assign<mongo::OperationContext*&, mongo::NamespaceString const&, mongo::LockMode> (this=0x7f0c83f400a0) at src/third_party/boost-1.60.0/boost/optional/optional.hpp:494
#7  emplace<mongo::OperationContext*&, mongo::NamespaceString const&, mongo::LockMode> (this=0x7f0c83f400a0) at src/third_party/boost-1.60.0/boost/optional/optional.hpp:981
#8  operator() (__closure=0x7f0c83f3ffc0) at src/mongo/db/ops/write_ops_exec.cpp:358
#9  insertBatchAndHandleErrors (out=0x7f0c83f3ffa0, lastOpFixer=0x7f0c83f3ff80, batch=..., wholeOp=..., opCtx=0x7f0c8ba3c400) at src/mongo/db/ops/write_ops_exec.cpp:371
#10 mongo::performInserts (opCtx=opCtx@entry=0x7f0c8ba3c400, wholeOp=...) at src/mongo/db/ops/write_ops_exec.cpp:534
#11 0x00007f0c84bb146e in mongo::(anonymous namespace)::CmdInsert::runImpl (this=<optimized out>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:315
#12 0x00007f0c84bab008 in mongo::(anonymous namespace)::WriteCommand::enhancedRun (this=<optimized out>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:229
#13 0x00007f0c85b73f3f in mongo::Command::publicRun (this=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands.cpp:387
#14 0x00007f0c84aef622 in runCommandImpl (startOperationTime=..., replyBuilder=0x7f0c8ba6f3c0, request=..., command=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f0c8ba3c400)
    at src/mongo/db/service_entry_point_mongod.cpp:506
#15 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7f0c8ba3c400, command=command@entry=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:760
#16 0x00007f0c84af0194 in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7f0c83f404f0) at src/mongo/db/service_entry_point_mongod.cpp:881
#17 0x00007f0c84af0194 in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#18 0x00007f0c84af0f30 in runCommands (message=..., opCtx=0x7f0c8ba3c400) at src/mongo/db/service_entry_point_mongod.cpp:891
#19 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7f0c8ba3c400, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1214
#20 0x00007f0c84afd95a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:455
#21 0x00007f0c84af8a9f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:532
#22 0x00007f0c84afc4de in operator() (__closure=0x7f0c882c87e0) at src/mongo/transport/service_state_machine.cpp:573
#23 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#24 0x00007f0c85a390e2 in operator() (this=0x7f0c83f42550) at /usr/local/include/c++/5.4.0/functional:2267
#25 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f0c882c1480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:125
#26 0x00007f0c84af768d in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f0c8ba64390, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:577
#27 0x00007f0c84afa031 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7f0c8ba64390, status=...) at src/mongo/transport/service_state_machine.cpp:358
#28 0x00007f0c84afac2b in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:314
#29 0x00007f0c84af8b31 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:529
#30 0x00007f0c84afc4de in operator() (__closure=0x7f0c882c8960) at src/mongo/transport/service_state_machine.cpp:573
#31 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#32 0x00007f0c85a39645 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#33 operator() (__closure=0x7f0c880d06e0) at src/mongo/transport/service_executor_synchronous.cpp:144
#34 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#35 0x00007f0c85f8a3c4 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#36 mongo::(anonymous namespace)::runFunc (ctx=0x7f0c882c8780) at src/mongo/transport/service_entry_point_utils.cpp:55
#37 0x00007f0c82c5de25 in start_thread () from /lib64/libpthread.so.0
#38 0x00007f0c8298b34d in clone () from /lib64/libc.so.6
(gdb) b /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/src/mongo/db/concurrency/d_concurrency.cpp:214
Breakpoint 2 at 0x7f0c85f5803f: file src/mongo/db/concurrency/d_concurrency.cpp, line 214.
(gdb) b /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/src/mongo/db/concurrency/d_concurrency.cpp:260
Breakpoint 3 at 0x7f0c85f5742e: file src/mongo/db/concurrency/d_concurrency.cpp, line 260.
(gdb) c
Continuing.

Breakpoint 2, mongo::Lock::DBLock::DBLock (this=0x7f0c83f3fd90, opCtx=0x7f0c8ba3c400, db=..., mode=<optimized out>) at src/mongo/db/concurrency/d_concurrency.cpp:214
214         invariant(LOCK_OK == _opCtx->lockState()->lock(_id, _mode)); //OperationContext::lockState->LockerImpl<>::lock
(gdb) bt
#0  mongo::Lock::DBLock::DBLock (this=0x7f0c83f3fd90, opCtx=0x7f0c8ba3c400, db=..., mode=<optimized out>) at src/mongo/db/concurrency/d_concurrency.cpp:214
#1  0x00007f0c84eefd14 in mongo::AutoGetCollection::AutoGetCollection (this=0x7f0c83f400a8, opCtx=0x7f0c8ba3c400, nss=..., modeDB=<optimized out>, modeColl=mongo::MODE_IX, viewMode=mongo::AutoGetCollection::kViewsForbidden)
    at src/mongo/db/db_raii.cpp:76
#2  0x00007f0c84bca2d2 in AutoGetCollection (modeAll=mongo::MODE_IX, nss=..., opCtx=0x7f0c8ba3c400, this=0x7f0c83f400a8) at src/mongo/db/db_raii.h:92
#3  emplace_assign<mongo::OperationContext*&, mongo::NamespaceString const&, mongo::LockMode> (this=0x7f0c83f400a0) at src/third_party/boost-1.60.0/boost/optional/optional.hpp:494
#4  emplace<mongo::OperationContext*&, mongo::NamespaceString const&, mongo::LockMode> (this=0x7f0c83f400a0) at src/third_party/boost-1.60.0/boost/optional/optional.hpp:981
#5  operator() (__closure=0x7f0c83f3ffc0) at src/mongo/db/ops/write_ops_exec.cpp:358
#6  insertBatchAndHandleErrors (out=0x7f0c83f3ffa0, lastOpFixer=0x7f0c83f3ff80, batch=..., wholeOp=..., opCtx=0x7f0c8ba3c400) at src/mongo/db/ops/write_ops_exec.cpp:371
#7  mongo::performInserts (opCtx=opCtx@entry=0x7f0c8ba3c400, wholeOp=...) at src/mongo/db/ops/write_ops_exec.cpp:534
#8  0x00007f0c84bb146e in mongo::(anonymous namespace)::CmdInsert::runImpl (this=<optimized out>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:315
#9  0x00007f0c84bab008 in mongo::(anonymous namespace)::WriteCommand::enhancedRun (this=<optimized out>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:229
#10 0x00007f0c85b73f3f in mongo::Command::publicRun (this=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands.cpp:387
#11 0x00007f0c84aef622 in runCommandImpl (startOperationTime=..., replyBuilder=0x7f0c8ba6f3c0, request=..., command=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f0c8ba3c400)
    at src/mongo/db/service_entry_point_mongod.cpp:506
#12 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7f0c8ba3c400, command=command@entry=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:760
#13 0x00007f0c84af0194 in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7f0c83f404f0) at src/mongo/db/service_entry_point_mongod.cpp:881
#14 0x00007f0c84af0194 in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#15 0x00007f0c84af0f30 in runCommands (message=..., opCtx=0x7f0c8ba3c400) at src/mongo/db/service_entry_point_mongod.cpp:891
#16 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7f0c8ba3c400, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1214
#17 0x00007f0c84afd95a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:455
#18 0x00007f0c84af8a9f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:532
#19 0x00007f0c84afc4de in operator() (__closure=0x7f0c882c87e0) at src/mongo/transport/service_state_machine.cpp:573
#20 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#21 0x00007f0c85a390e2 in operator() (this=0x7f0c83f42550) at /usr/local/include/c++/5.4.0/functional:2267
#22 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f0c882c1480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:125
#23 0x00007f0c84af768d in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f0c8ba64390, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:577
#24 0x00007f0c84afa031 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7f0c8ba64390, status=...) at src/mongo/transport/service_state_machine.cpp:358
#25 0x00007f0c84afac2b in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:314
#26 0x00007f0c84af8b31 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:529
#27 0x00007f0c84afc4de in operator() (__closure=0x7f0c882c8960) at src/mongo/transport/service_state_machine.cpp:573
#28 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#29 0x00007f0c85a39645 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#30 operator() (__closure=0x7f0c880d06e0) at src/mongo/transport/service_executor_synchronous.cpp:144
#31 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#32 0x00007f0c85f8a3c4 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#33 mongo::(anonymous namespace)::runFunc (ctx=0x7f0c882c8780) at src/mongo/transport/service_entry_point_utils.cpp:55
#34 0x00007f0c82c5de25 in start_thread () from /lib64/libpthread.so.0
#35 0x00007f0c8298b34d in clone () from /lib64/libc.so.6
(gdb) c
Continuing.

Breakpoint 3, mongo::Lock::CollectionLock::CollectionLock (this=0x7f0c83f40140, lockState=0x7f0c8ba77000, ns=..., mode=mongo::MODE_IX) at src/mongo/db/concurrency/d_concurrency.cpp:260
260         if (supportsDocLocking()) {
(gdb) bt
#0  mongo::Lock::CollectionLock::CollectionLock (this=0x7f0c83f40140, lockState=0x7f0c8ba77000, ns=..., mode=mongo::MODE_IX) at src/mongo/db/concurrency/d_concurrency.cpp:260
#1  0x00007f0c84eef875 in mongo::AutoGetCollection::AutoGetCollection (this=0x7f0c83f400a8, opCtx=0x7f0c8ba3c400, nss=..., modeColl=mongo::MODE_IX, viewMode=<optimized out>, lock=...) at src/mongo/db/db_raii.cpp:86
#2  0x00007f0c84eefd2f in mongo::AutoGetCollection::AutoGetCollection (this=0x7f0c83f400a8, opCtx=0x7f0c8ba3c400, nss=..., modeDB=<optimized out>, modeColl=mongo::MODE_IX, viewMode=mongo::AutoGetCollection::kViewsForbidden)
    at src/mongo/db/db_raii.cpp:76
#3  0x00007f0c84bca2d2 in AutoGetCollection (modeAll=mongo::MODE_IX, nss=..., opCtx=0x7f0c8ba3c400, this=0x7f0c83f400a8) at src/mongo/db/db_raii.h:92
#4  emplace_assign<mongo::OperationContext*&, mongo::NamespaceString const&, mongo::LockMode> (this=0x7f0c83f400a0) at src/third_party/boost-1.60.0/boost/optional/optional.hpp:494
#5  emplace<mongo::OperationContext*&, mongo::NamespaceString const&, mongo::LockMode> (this=0x7f0c83f400a0) at src/third_party/boost-1.60.0/boost/optional/optional.hpp:981
#6  operator() (__closure=0x7f0c83f3ffc0) at src/mongo/db/ops/write_ops_exec.cpp:358
#7  insertBatchAndHandleErrors (out=0x7f0c83f3ffa0, lastOpFixer=0x7f0c83f3ff80, batch=..., wholeOp=..., opCtx=0x7f0c8ba3c400) at src/mongo/db/ops/write_ops_exec.cpp:371
#8  mongo::performInserts (opCtx=opCtx@entry=0x7f0c8ba3c400, wholeOp=...) at src/mongo/db/ops/write_ops_exec.cpp:534
#9  0x00007f0c84bb146e in mongo::(anonymous namespace)::CmdInsert::runImpl (this=<optimized out>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:315
#10 0x00007f0c84bab008 in mongo::(anonymous namespace)::WriteCommand::enhancedRun (this=<optimized out>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:229
#11 0x00007f0c85b73f3f in mongo::Command::publicRun (this=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f0c8ba3c400, request=..., result=...) at src/mongo/db/commands.cpp:387
#12 0x00007f0c84aef622 in runCommandImpl (startOperationTime=..., replyBuilder=0x7f0c8ba6f3c0, request=..., command=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7f0c8ba3c400)
    at src/mongo/db/service_entry_point_mongod.cpp:506
#13 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7f0c8ba3c400, command=command@entry=0x7f0c86e253a0 <mongo::(anonymous namespace)::cmdInsert>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:760
#14 0x00007f0c84af0194 in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7f0c83f404f0) at src/mongo/db/service_entry_point_mongod.cpp:881
#15 0x00007f0c84af0194 in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#16 0x00007f0c84af0f30 in runCommands (message=..., opCtx=0x7f0c8ba3c400) at src/mongo/db/service_entry_point_mongod.cpp:891
#17 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7f0c8ba3c400, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1214
#18 0x00007f0c84afd95a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:455
#19 0x00007f0c84af8a9f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:532
#20 0x00007f0c84afc4de in operator() (__closure=0x7f0c882c87e0) at src/mongo/transport/service_state_machine.cpp:573
#21 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#22 0x00007f0c85a390e2 in operator() (this=0x7f0c83f42550) at /usr/local/include/c++/5.4.0/functional:2267
#23 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f0c882c1480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:125
#24 0x00007f0c84af768d in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f0c8ba64390, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:577
#25 0x00007f0c84afa031 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7f0c8ba64390, status=...) at src/mongo/transport/service_state_machine.cpp:358
#26 0x00007f0c84afac2b in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:314
#27 0x00007f0c84af8b31 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f0c8ba64390, guard=...) at src/mongo/transport/service_state_machine.cpp:529
#28 0x00007f0c84afc4de in operator() (__closure=0x7f0c882c8960) at src/mongo/transport/service_state_machine.cpp:573
#29 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#30 0x00007f0c85a39645 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#31 operator() (__closure=0x7f0c880d06e0) at src/mongo/transport/service_executor_synchronous.cpp:144
#32 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#33 0x00007f0c85f8a3c4 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#34 mongo::(anonymous namespace)::runFunc (ctx=0x7f0c882c8780) at src/mongo/transport/service_entry_point_utils.cpp:55
*/

}  // namespace mongo
