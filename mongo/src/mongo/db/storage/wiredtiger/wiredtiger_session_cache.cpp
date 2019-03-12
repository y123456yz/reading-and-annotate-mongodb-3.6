// wiredtiger_session_cache.cpp

/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#include "mongo/platform/basic.h"

#include "mongo/db/storage/wiredtiger/wiredtiger_session_cache.h"

#include "mongo/base/error_codes.h"
#include "mongo/db/mongod_options.h"
#include "mongo/db/repl/repl_settings.h"
#include "mongo/db/storage/journal_listener.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_kv_engine.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_util.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/thread.h"
#include "mongo/util/log.h"
#include "mongo/util/scopeguard.h"

namespace mongo {
/*
wiredtiger简单例子:
error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &conn));

//__conn_open_session
error_check(conn->open_session(conn, NULL, NULL, &session));

//__session_create	创建table表
error_check(session->create(
	session, "table:access", "key_format=S,value_format=S"));

//__session_open_cursor  //获取一个cursor通过cursorp返回
error_check(session->open_cursor(
	session, "table:access", NULL, NULL, &cursor));

//__wt_cursor_set_key
cursor->set_key(cursor, "key1");	
//__wt_cursor_set_value
cursor->set_value(cursor, "value1");
//__curfile_insert
error_check(cursor->insert(cursor));
*/
//WiredTigerKVEngine::WiredTigerKVEngine中wiredtiger_open获取到的conn
//WiredTigerSession::WiredTigerSession中conn->open_session获取到的session

/*
db/storage/wiredtiger/wiredtiger_index.cpp:    WiredTigerSession session(WiredTigerRecoveryUnit::get(opCtx)->getSessionCache()->conn());
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:                    UniqueWiredTigerSession session = _sessionCache->getSession();
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession sessionWrapper(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_session_cache.cpp:        UniqueWiredTigerSession session = getSession();
*/ //上面的注释这些地方会构造使用
//WiredTigerSessionCache::getSession中执行
WiredTigerSession::WiredTigerSession(WT_CONNECTION* conn, uint64_t epoch, uint64_t cursorEpoch)
    : _epoch(epoch),
      _cursorEpoch(cursorEpoch),
      _session(NULL),
      _cursorGen(0),
      _cursorsCached(0),
      _cursorsOut(0) {
    invariantWTOK(conn->open_session(conn, NULL, "isolation=snapshot", &_session)); //默认隔离级别
}

//WiredTigerSessionCache::getSession中执行
/*
#0  mongo::WiredTigerSession::WiredTigerSession (this=0x7f5e38dfa0a0, conn=0x7f5e3558f000, cache=0x7f5e35573dc0, epoch=0, cursorEpoch=0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:98
#1  0x00007f5e309b6820 in mongo::WiredTigerSessionCache::getSession (this=0x7f5e35573dc0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:392
#2  0x00007f5e309b3ebd in mongo::WiredTigerRecoveryUnit::_ensureSession (this=this@entry=0x7f5e38dfe900) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:140
#3  0x00007f5e309b4031 in _ensureSession (this=0x7f5e38dfe900) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:171
#4  mongo::WiredTigerRecoveryUnit::getSessionNoTxn (this=0x7f5e38dfe900) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:172
#5  0x00007f5e30988f73 in mongo::WiredTigerServerStatusSection::generateSection (this=<optimized out>, opCtx=0x7f5e38d52b80, configElement=...) at src/mongo/db/storage/wiredtiger/wiredtiger_server_status.cpp:65


#0  mongo::WiredTigerSession::WiredTigerSession (this=0x7f5e38dfacd0, conn=0x7f5e3558f000, cache=0x7f5e35573dc0, epoch=0, cursorEpoch=0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:98
#1  0x00007f5e309b6820 in mongo::WiredTigerSessionCache::getSession (this=0x7f5e35573dc0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:392
#2  0x00007f5e309b3ebd in mongo::WiredTigerRecoveryUnit::_ensureSession (this=0x7f5e38dfe280) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:140
#3  0x00007f5e309b4cf5 in _ensureSession (this=0x7f5e38dfe280) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:276
#4  mongo::WiredTigerRecoveryUnit::_txnOpen (this=this@entry=0x7f5e38dfe280) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:277
#5  0x00007f5e309b5367 in getSession (this=<optimized out>) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:165
#6  mongo::WiredTigerCursor::WiredTigerCursor (this=0x7f5e2710c260, uri=..., tableId=1, forRecordStore=<optimized out>, opCtx=<optimized out>) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:347
#7  0x00007f5e309a7475 in mongo::WiredTigerRecordStore::findRecord
*/
/*
db/storage/wiredtiger/wiredtiger_index.cpp:    WiredTigerSession session(WiredTigerRecoveryUnit::get(opCtx)->getSessionCache()->conn());
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:                    UniqueWiredTigerSession session = _sessionCache->getSession();
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession sessionWrapper(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_kv_engine.cpp:    WiredTigerSession session(_conn);
db/storage/wiredtiger/wiredtiger_session_cache.cpp:        UniqueWiredTigerSession session = getSession();
*/ //上面的注释这些地方会构造使用
WiredTigerSession::WiredTigerSession(WT_CONNECTION* conn,
                                     WiredTigerSessionCache* cache,
                                     uint64_t epoch,
                                     uint64_t cursorEpoch)
    : _epoch(epoch),
      _cursorEpoch(cursorEpoch),
      _cache(cache),
      _session(NULL),
      _cursorGen(0),
      _cursorsCached(0),
      _cursorsOut(0) {
    invariantWTOK(conn->open_session(conn, NULL, "isolation=snapshot", &_session));
}

//注意和WiredTigerSessionCache::WiredTigerSessionDeleter::operator()的区别
WiredTigerSession::~WiredTigerSession() {
    if (_session) {
        invariantWTOK(_session->close(_session, NULL));
    }
}

/*
The WT_SESSION::open_cursor overwrite configuration is true by default, causing WT_CURSOR::insert, 
WT_CURSOR::remove and WT_CURSOR::update to ignore the current state of the record, and these methods 
will succeed regardless of whether or not the record previously exists.

When an application configures overwrite to false, WT_CURSOR::insert will fail with WT_DUPLICATE_KEY 
if the record previously exists, and WT_CURSOR::update and WT_CURSOR::remove will fail with WT_NOTFOUND 
if the record does not previously exist.
*/

//获取cursor  同时该获取到的新c姜从_cursors列表中去除
WT_CURSOR* WiredTigerSession::getCursor(const std::string& uri, uint64_t id, bool forRecordStore) {
    // Find the most recently used cursor  
    for (CursorCache::iterator i = _cursors.begin(); i != _cursors.end(); ++i) {
        if (i->_id == id) {
            WT_CURSOR* c = i->_cursor;
            _cursors.erase(i);
            _cursorsOut++;
            _cursorsCached--;
            return c;
        }
    }

    WT_CURSOR* c = NULL; 
    int ret = _session->open_cursor( //如果false则重复的话报错WT_DUPLICATE_KEY，如果为ture则始终成功写入
        _session, uri.c_str(), NULL, forRecordStore ? "" : "overwrite=false", &c);
    if (ret != ENOENT)
        invariantWTOK(ret);
    if (c)
        _cursorsOut++;
    return c;
}

//把cursor加入到_cursors列表
void WiredTigerSession::releaseCursor(uint64_t id, WT_CURSOR* cursor) {
    invariant(_session);
    invariant(cursor);
    _cursorsOut--;

    invariantWTOK(cursor->reset(cursor));

    // Cursors are pushed to the front of the list and removed from the back
    _cursors.push_front(WiredTigerCachedCursor(id, _cursorGen++, cursor));
    _cursorsCached++;

    // "Old" is defined as not used in the last N**2 operations, if we have N cursors cached.
    // The reasoning here is to imagine a workload with N tables performing operations randomly
    // across all of them (i.e., each cursor has 1/N chance of used for each operation).  We
    // would like to cache N cursors in that case, so any given cursor could go N**2 operations
    // in between use.
    while (_cursorGen - _cursors.back()._gen > 10000) {
        cursor = _cursors.back()._cursor;
        _cursors.pop_back();
        _cursorsCached--;
        invariantWTOK(cursor->close(cursor));
    }
}

//erase _cursors中cursor->uri为uri的c
//WiredTigerSessionCache::closeAllCursors   openBulkCursor中执行
void WiredTigerSession::closeAllCursors(const std::string& uri) {
    invariant(_session);

    bool all = (uri == "");
    for (auto i = _cursors.begin(); i != _cursors.end();) {
        WT_CURSOR* cursor = i->_cursor;
        if (cursor && (all || uri == cursor->uri)) {
            invariantWTOK(cursor->close(cursor));
            i = _cursors.erase(i);
        } else
            ++i;
    }
}

//WiredTigerSessionCache::closeCursorsForQueuedDrops()调用
void WiredTigerSession::closeCursorsForQueuedDrops(WiredTigerKVEngine* engine) {
    invariant(_session);

    _cursorEpoch = _cache->getCursorEpoch();
	//WiredTigerKVEngine::filterCursorsWithQueuedDrops 找出需要drop的 cursor
    auto toDrop = engine->filterCursorsWithQueuedDrops(&_cursors);

    for (auto i = toDrop.begin(); i != toDrop.end(); i++) {
        WT_CURSOR* cursor = i->_cursor;
        if (cursor) {
            invariantWTOK(cursor->close(cursor));
        }
    }
}

namespace {
AtomicUInt64 nextTableId(1);
}
// static   WiredTigerIndex::WiredTigerIndex
uint64_t WiredTigerSession::genTableId() {
    return nextTableId.fetchAndAdd(1);
}

// -----------------------
//WiredTigerKVEngine::WiredTigerKVEngine中调用构造该类
WiredTigerSessionCache::WiredTigerSessionCache(WiredTigerKVEngine* engine)
    : _engine(engine), _conn(engine->getConnection()), _snapshotManager(_conn), _shuttingDown(0) {}

WiredTigerSessionCache::WiredTigerSessionCache(WT_CONNECTION* conn)
    : _engine(NULL), _conn(conn), _snapshotManager(_conn), _shuttingDown(0) {}

WiredTigerSessionCache::~WiredTigerSessionCache() {
    shuttingDown();
}

/* CTRL+C退出程序的时候会走这里
(gdb) bt 
#0  mongo::WiredTigerSessionCache::closeAll (this=this@entry=0x7f3df729edc0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:368
#1  0x00007f3df3e1d6cd in mongo::WiredTigerSessionCache::shuttingDown (this=0x7f3df729edc0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:254
#2  0x00007f3df3e028c8 in mongo::WiredTigerKVEngine::cleanShutdown (this=0x7f3df6f71680) at src/mongo/db/storage/wiredtiger/wiredtiger_kv_engine.cpp:510
#3  0x00007f3df3fd9a8e in mongo::ServiceContextMongoD::shutdownGlobalStorageEngineCleanly (this=0x7f3df6eb4480) at src/mongo/db/service_context_d.cpp:239
#4  0x00007f3df3dc2b1e in mongo::(anonymous namespace)::shutdownTask () at src/mongo/db/db.cpp:1385
#5  0x00007f3df55aff92 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#6  mongo::(anonymous namespace)::runTasks(std::stack<std::function<void()>, std::deque<std::function<void()>, std::allocator<std::function<void()> > > >) (tasks=...) at src/mongo/util/exit.cpp:61
#7  0x00007f3df3d546f3 in mongo::shutdown (code=code@entry=mongo::EXIT_CLEAN) at src/mongo/util/exit.cpp:140
#8  0x00007f3df45939d2 in exitCleanly (code=mongo::EXIT_CLEAN) at src/mongo/util/exit.h:81
#9  mongo::(anonymous namespace)::signalProcessingThread (rotate=mongo::kNeedToRotateLogFile) at src/mongo/util/signal_handlers.cpp:198
#10 0x00007f3df29328f0 in std::execute_native_thread_routine (__p=<optimized out>) at ../../../.././libstdc++-v3/src/c++11/thread.cc:84
#11 0x00007f3df214ee25 in start_thread () from /lib64/libpthread.so.0
#12 0x00007f3df1e7c34d in clone () from /lib64/libc.so.6
*/ //CTRL+C退出程序的时候会走这里

//WiredTigerSessionCache::~WiredTigerSessionCache中调用
void WiredTigerSessionCache::shuttingDown() {
    uint32_t actual = _shuttingDown.load();
    uint32_t expected;

    // Try to atomically set _shuttingDown flag, but just return if another thread was first.
    do {
        expected = actual;
        actual = _shuttingDown.compareAndSwap(expected, expected | kShuttingDownMask);
        if (actual & kShuttingDownMask)
            return;
    } while (actual != expected);

    // Spin as long as there are threads in releaseSession
    while (_shuttingDown.load() != kShuttingDownMask) {
        sleepmillis(1);
    }

    closeAll();
    _snapshotManager.shutdown(); //WiredTigerSnapshotManager::shutdown
}

void WiredTigerSessionCache::waitUntilDurable(bool forceCheckpoint, bool stableCheckpoint) {
    // For inMemory storage engines, the data is "as durable as it's going to get".
    // That is, a restart is equivalent to a complete node failure.
    if (isEphemeral()) {
        return;
    }

    const int shuttingDown = _shuttingDown.fetchAndAdd(1);
    ON_BLOCK_EXIT([this] { _shuttingDown.fetchAndSubtract(1); });

    uassert(ErrorCodes::ShutdownInProgress,
            "Cannot wait for durability because a shutdown is in progress",
            !(shuttingDown & kShuttingDownMask));

    // Stable checkpoints are only meaningful in a replica set. Replication sets the "stable
    // timestamp". If the stable timestamp is unset, WiredTiger takes a full checkpoint, which is
    // incidentally what we want. A "true" stable checkpoint (a stable timestamp was set on the
    // WT_CONNECTION, i.e: replication is on) requires `forceCheckpoint` to be true and journaling
    // to be enabled.
    if (stableCheckpoint && getGlobalReplSettings().usingReplSets()) {
        invariant(forceCheckpoint && _engine->isDurable());
    }

    // When forcing a checkpoint with journaling enabled, don't synchronize with other
    // waiters, as a log flush is much cheaper than a full checkpoint.
    if (forceCheckpoint && _engine->isDurable()) {
        UniqueWiredTigerSession session = getSession();
        WT_SESSION* s = session->getSession();
        {
            stdx::unique_lock<stdx::mutex> lk(_journalListenerMutex);
            JournalListener::Token token = _journalListener->getToken();
            const bool keepOldBehavior = true;
            if (keepOldBehavior) {
                invariantWTOK(s->checkpoint(s, nullptr));
            } else {
                std::string config =
                    stableCheckpoint ? "use_timestamp=true" : "use_timestamp=false";
                invariantWTOK(s->checkpoint(s, config.c_str()));
            }
            _journalListener->onDurable(token);
        }
        LOG(4) << "created checkpoint (forced)";
        return;
    }

    uint32_t start = _lastSyncTime.load();
    // Do the remainder in a critical section that ensures only a single thread at a time
    // will attempt to synchronize.
    stdx::unique_lock<stdx::mutex> lk(_lastSyncMutex);
    uint32_t current = _lastSyncTime.loadRelaxed();  // synchronized with writes through mutex
    if (current != start) {
        // Someone else synced already since we read lastSyncTime, so we're done!
        return;
    }
    _lastSyncTime.store(current + 1);

    // Nobody has synched yet, so we have to sync ourselves.

    // This gets the token (OpTime) from the last write, before flushing (either the journal, or a
    // checkpoint), and then reports that token (OpTime) as a durable write.
    stdx::unique_lock<stdx::mutex> jlk(_journalListenerMutex);
    JournalListener::Token token = _journalListener->getToken();

    // Initialize on first use.
    if (!_waitUntilDurableSession) {
        invariantWTOK(
            _conn->open_session(_conn, NULL, "isolation=snapshot", &_waitUntilDurableSession));
    }

    // Use the journal when available, or a checkpoint otherwise.
    if (_engine && _engine->isDurable()) { //对应wiredtiger中的log日志模块
        invariantWTOK(_waitUntilDurableSession->log_flush(_waitUntilDurableSession, "sync=on"));
        LOG(4) << "flushed journal";
    } else { //对应checkpoint模块
        invariantWTOK(_waitUntilDurableSession->checkpoint(_waitUntilDurableSession, NULL));
        LOG(4) << "created checkpoint";
    }

	//ReplicationCoordinatorExternalStateImpl::onDurable
    _journalListener->onDurable(token);
}

void WiredTigerSessionCache::closeAllCursors(const std::string& uri) {
    stdx::lock_guard<stdx::mutex> lock(_cacheLock);
    for (SessionCache::iterator i = _sessions.begin(); i != _sessions.end(); i++) {
        (*i)->closeAllCursors(uri); //WiredTigerSession::closeAllCursors
    }
}

//WiredTigerSessionCache::releaseSession   WiredTigerKVEngine::dropIdent
void WiredTigerSessionCache::closeCursorsForQueuedDrops() {
    // Increment the cursor epoch so that all cursors from this epoch are closed.
    _cursorEpoch.fetchAndAdd(1);

    stdx::lock_guard<stdx::mutex> lock(_cacheLock);
    for (SessionCache::iterator i = _sessions.begin(); i != _sessions.end(); i++) {
        (*i)->closeCursorsForQueuedDrops(_engine); //WiredTigerSession::closeCursorsForQueuedDrops
    }
}

//删除所有WiredTigerSession _sessions      WiredTigerSessionCache::shuttingDown调用
void WiredTigerSessionCache::closeAll() {
    // Increment the epoch as we are now closing all sessions with this epoch.
    SessionCache swap;

    {
        stdx::lock_guard<stdx::mutex> lock(_cacheLock);
        _epoch.fetchAndAdd(1);
        _sessions.swap(swap);
    }

    for (SessionCache::iterator i = swap.begin(); i != swap.end(); i++) {
        delete (*i);
    }
}

/*
ephemeralForTest存储引擎（ephemeralForTest Storage Engine）

MongoDB 3.2提供了一个新的用于测试的存储引擎。而不是一些元数据，用于测试的存储引擎不维护
任何磁盘数据，不需要在测试运行期间做清理。用于测试的存储引擎是无支持的。
*/
bool WiredTigerSessionCache::isEphemeral() {
    return _engine && _engine->isEphemeral(); 
}

/*
以下接口调用getSession
WiredTigerKVEngine::flushAllFiles
WiredTigerKVEngine::WiredTigerCheckpointThread
WiredTigerKVEngine::WiredTigerCheckpointThread
WiredTigerKVEngine::WiredTigerJournalFlusher
WiredTigerOplogManager::_oplogJournalThreadLoop
WiredTigerRecoveryUnit::waitUntilDurable
MultiIndexBlockImpl::init
WiredTigerRecoveryUnit::_ensureSession
*/ //选择是直接new新的UniqueWiredTigerSession还是直接使用WiredTigerSessionCache._sessions缓存中的session
UniqueWiredTigerSession WiredTigerSessionCache::getSession() {
    // We should never be able to get here after _shuttingDown is set, because no new
    // operations should be allowed to start.
    invariant(!(_shuttingDown.loadRelaxed() & kShuttingDownMask));

    {
        stdx::lock_guard<stdx::mutex> lock(_cacheLock);
        if (!_sessions.empty()) { //WiredTigerSession _sessions不为空，则直接取其中一个
            // Get the most recently used session so that if we discard sessions, we're
            // discarding older ones  
            WiredTigerSession* cachedSession = _sessions.back();
            _sessions.pop_back(); //WiredTigerSessionCache._sessions
            return UniqueWiredTigerSession(cachedSession); 
        }
    }

    // Outside of the cache partition lock, but on release will be put back on the cache
    return UniqueWiredTigerSession( //调用wiredtiger conn->open_session获取新的session
        new WiredTigerSession(_conn, this, _epoch.load(), _cursorEpoch.load()));
}

/*
#0  mongo::WiredTigerSessionCache::releaseSession (this=0x7ff8d6abfdc0, session=0x7ff8da37aaf0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:441
#1  0x00007ff8d2c6aa41 in mongo::WiredTigerRecoveryUnit::~WiredTigerRecoveryUnit (this=0x7ff8da38fb80, __in_chrg=<optimized out>) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:71
#2  0x00007ff8d42c4046 in operator() (this=<optimized out>, __ptr=<optimized out>) at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:76
#3  ~unique_ptr (this=0x7ff8da31c470, __in_chrg=<optimized out>) at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:236
#4  ~OperationContext (this=0x7ff8da31c400, __in_chrg=<optimized out>) at src/mongo/db/operation_context.h:124
#5  ~OperationContext (this=0x7ff8da31c400, __in_chrg=<optimized out>) at src/mongo/db/operation_context.h:124
#6  mongo::ServiceContext::OperationContextDeleter::operator() (this=<optimized out>, opCtx=0x7ff8da31c400) at src/mongo/db/service_context.cpp:300

#0  mongo::WiredTigerSessionCache::releaseSession (this=0x7ff8d6abfdc0, session=0x7ff8da1c20f0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:441
#1  0x00007ff8d2c6cc1e in mongo::WiredTigerSessionCache::WiredTigerSessionDeleter::operator() (this=this@entry=0x7ff8cb3c69e0, session=<optimized out>) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:501
#2  0x00007ff8d2c59204 in ~unique_ptr (this=0x7ff8cb3c69e0, __in_chrg=<optimized out>) at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:236
#3  mongo::WiredTigerKVEngine::WiredTigerCheckpointThread::run (this=0x7ff8d6ad1640) at src/mongo/db/storage/wiredtiger/wiredtiger_kv_engine.cpp:174
#4  0x00007ff8d42ef5b1 in mongo::BackgroundJob::jobBody

(gdb) next
78      WiredTigerRecoveryUnit::~WiredTigerRecoveryUnit() {
(gdb) next
mongo::WiredTigerSessionCache::WiredTigerSessionDeleter::operator() (this=0x7f410e1a1518, session=0x7f410e173be0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:516
516         WiredTigerSession* session) const {
(gdb) next
517         session->_cache->releaseSession(session);
(gdb) next
516         WiredTigerSession* session) const {
(gdb) next
518     }
(gdb) next
517         session->_cache->releaseSession(session);
(gdb) next
mongo::WiredTigerSessionCache::releaseSession (this=0x7f410a8d4dc0, session=0x7f410e173be0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:451
451     void WiredTigerSessionCache::releaseSession(WiredTigerSession* session) {
(gdb) next

Breakpoint 1, mongo::WiredTigerSessionCache::releaseSession (this=0x7f410a8d4dc0, session=0x7f410e173be0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:451
451     void WiredTigerSessionCache::releaseSession(WiredTigerSession* session) {
(gdb) bt
#0  mongo::WiredTigerSessionCache::releaseSession (this=0x7f410a8d4dc0, session=0x7f410e173be0) at src/mongo/db/storage/wiredtiger/wiredtiger_session_cache.cpp:451
这里应该还有个WiredTigerSessionCache::WiredTigerSessionDeleter::operator，见上面的step调试
#1  0x00007f410689ca41 in mongo::WiredTigerRecoveryUnit::~WiredTigerRecoveryUnit (this=0x7f410e1a1500, __in_chrg=<optimized out>) at src/mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.cpp:81
#2  0x00007f4107ef6046 in operator() (this=<optimized out>, __ptr=<optimized out>) at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:76
#3  ~unique_ptr (this=0x7f410e12e1f0, __in_chrg=<optimized out>) at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:236
#4  ~OperationContext (this=0x7f410e12e180, __in_chrg=<optimized out>) at src/mongo/db/operation_context.h:124
#5  ~OperationContext (this=0x7f410e12e180, __in_chrg=<optimized out>) at src/mongo/db/operation_context.h:124
#6  mongo::ServiceContext::OperationContextDeleter::operator() 

*/
//WiredTigerSessionCache::WiredTigerSessionDeleter::operator()   WiredTigerRecoveryUnit::~WiredTigerRecoveryUnit(也会调用WiredTigerSessionCache::WiredTigerSessionDeleter::operator()然后走到这里)
//release的session可以暂存到_sessions来做复用
void WiredTigerSessionCache::releaseSession(WiredTigerSession* session) {
    invariant(session);
    invariant(session->cursorsOut() == 0);

    const int shuttingDown = _shuttingDown.fetchAndAdd(1);
    ON_BLOCK_EXIT([this] { _shuttingDown.fetchAndSubtract(1); });

    if (shuttingDown & kShuttingDownMask) {
        // There is a race condition with clean shutdown, where the storage engine is ripped from
        // underneath OperationContexts, which are not "active" (i.e., do not have any locks), but
        // are just about to delete the recovery unit. See SERVER-16031 for more information. Since
        // shutting down the WT_CONNECTION will close all WT_SESSIONS, we shouldn't also try to
        // directly close this session.
        session->_session = nullptr;  // Prevents calling _session->close() in destructor.
        delete session;
        return;
    }

    {
        WT_SESSION* ss = session->getSession();
        uint64_t range;
        // This checks that we are only caching idle sessions and not something which might hold
        // locks or otherwise prevent truncation.
        invariantWTOK(ss->transaction_pinned_range(ss, &range));
        invariant(range == 0);

        // Release resources in the session we're about to cache.
        invariantWTOK(ss->reset(ss));
    }

    // If the cursor epoch has moved on, close all cursors in the session.
    uint64_t cursorEpoch = _cursorEpoch.load();
    if (session->_getCursorEpoch() != cursorEpoch)
        session->closeCursorsForQueuedDrops(_engine);

    bool returnedToCache = false;
    uint64_t currentEpoch = _epoch.load();

	//把该session放入cache做复用还是直接drop掉
    if (session->_getEpoch() == currentEpoch) {  // check outside of lock to reduce contention
        stdx::lock_guard<stdx::mutex> lock(_cacheLock);
        if (session->_getEpoch() == _epoch.load()) {  // recheck inside the lock for correctness
            returnedToCache = true; //缓存起来
            _sessions.push_back(session);
        }
    } else
        invariant(session->_getEpoch() < currentEpoch);

    if (!returnedToCache)
        delete session;

	
    if (_engine && _engine->haveDropsQueued()) //WiredTigerKVEngine::haveDropsQueued
        _engine->dropSomeQueuedIdents(); //WiredTigerKVEngine::dropSomeQueuedIdents
}

//WiredTigerKVEngine::setJournalListener中调用
void WiredTigerSessionCache::setJournalListener(JournalListener* jl) {
    stdx::unique_lock<stdx::mutex> lk(_journalListenerMutex);
    _journalListener = jl;
}

//释放session对应的_cache 有可能不是直接close，而是放入WiredTigerSessionCache._sessions
//WiredTigerRecoveryUnit::~WiredTigerRecoveryUnit中调用
//WiredTigerCheckpointThread::run中的UniqueWiredTigerSession session = _sessionCache->getSession();临时变量生命周期执行完后调用
void WiredTigerSessionCache::WiredTigerSessionDeleter::operator()(
    WiredTigerSession* session) const {
    session->_cache->releaseSession(session);
}

}  // namespace mongo

