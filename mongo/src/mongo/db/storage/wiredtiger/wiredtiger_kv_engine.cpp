// wiredtiger_kv_engine.cpp

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

#ifdef _WIN32
#define NVALGRIND
#endif

#include <memory>

#include "mongo/db/storage/wiredtiger/wiredtiger_kv_engine.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <valgrind/valgrind.h>

#include "mongo/base/error_codes.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/bson/dotted_path_support.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/collection_catalog_entry.h"
#include "mongo/db/client.h"
#include "mongo/db/commands/server_status_metric.h"
#include "mongo/db/concurrency/locker.h"
#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/mongod_options.h"
#include "mongo/db/repl/repl_settings.h"
#include "mongo/db/server_options.h"
#include "mongo/db/server_parameters.h"
#include "mongo/db/service_context.h"
#include "mongo/db/storage/journal_listener.h"
#include "mongo/db/storage/storage_options.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_customization_hooks.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_extensions.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_global_options.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_index.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_record_store.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_session_cache.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_size_storer.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_util.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/background.h"
#include "mongo/util/concurrency/idle_thread_block.h"
#include "mongo/util/concurrency/ticketholder.h"
#include "mongo/util/exit.h"
#include "mongo/util/log.h"
#include "mongo/util/processinfo.h"
#include "mongo/util/scopeguard.h"
#include "mongo/util/time_support.h"

//wiredtiger中的wt文件通过以下方式打开wt内容:wt -C "extensions=[/usr/local/lib/libwiredtiger_snappy.so]" -h . dump table:_mdb_catalog

//wiredtiger使用可以参考: MongoDB如何使用wiredTiger？
//https://mongoing.com/archives/2214
//WT元数据文件管理：https://cloud.tencent.com/developer/article/1626996
//官方wiredtiger example例子参考:
// https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0/tree/master/wiredtiger/examples/c
#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

namespace mongo {

using std::set;
using std::string;

namespace dps = ::mongo::dotted_path_support;

//WiredTigerKVEngine::WiredTigerKVEngine中初始化调用
//WiredTigerKVEngine._journalFlusher成员为该类
class WiredTigerKVEngine::WiredTigerJournalFlusher : public BackgroundJob {
public:
    explicit WiredTigerJournalFlusher(WiredTigerSessionCache* sessionCache)
        : BackgroundJob(false /* deleteSelf */), _sessionCache(sessionCache) {}

    virtual string name() const {
        return "WTJournalFlusher";
    }

    virtual void run() {
        Client::initThread(name().c_str());

        LOG(1) << "starting " << name() << " thread";

        while (!_shuttingDown.load()) {
            try {
                const bool forceCheckpoint = false;
                const bool stableCheckpoint = false;
                _sessionCache->waitUntilDurable(forceCheckpoint, stableCheckpoint);
            } catch (const AssertionException& e) {
                invariant(e.code() == ErrorCodes::ShutdownInProgress);
            }

            int ms = storageGlobalParams.journalCommitIntervalMs.load();
            if (!ms) {
                ms = 100;
            }

            MONGO_IDLE_THREAD_BLOCK;
            sleepmillis(ms);
        }
        LOG(1) << "stopping " << name() << " thread";
    }

    void shutdown() {
        _shuttingDown.store(true);
        wait();
    }

private:
    WiredTigerSessionCache* _sessionCache;
    AtomicBool _shuttingDown{false};
};

//WiredTigerKVEngine::WiredTigerKVEngine中调用执行
//WiredTigerKVEngine._checkpointThread成员为该类
//定期做checkpoint操作
/*
MongoDB 有一个后台线程（WTCheckpointThread）会定期（默认情况下每 60 秒，由 storage.syncPeriodSecs 配置
决定）根据 stable timestamp 触发新的 checkpoint 创建，这个 checkpoint 在实现中被称为「stable checkpoint」。
参考https://mongoing.com/archives/77853
*/
class WiredTigerKVEngine::WiredTigerCheckpointThread : public BackgroundJob {
public:
    explicit WiredTigerCheckpointThread(WiredTigerSessionCache* sessionCache)
        : BackgroundJob(false /* deleteSelf */),
          _sessionCache(sessionCache),
          _stableTimestamp(0),
          _initialDataTimestamp(0) {}

    virtual string name() const {
        return "WTCheckpointThread";
    }

    virtual void run() {
        Client::initThread(name().c_str());

        LOG(1) << "starting " << name() << " thread";

        while (!_shuttingDown.load()) {
            {
                stdx::unique_lock<stdx::mutex> lock(_mutex);
                MONGO_IDLE_THREAD_BLOCK;
                _condvar.wait_for(lock,
                                  stdx::chrono::seconds(static_cast<std::int64_t>(
                                      wiredTigerGlobalOptions.checkpointDelaySecs)));
            }

            const Timestamp stableTimestamp(_stableTimestamp.load());
            const Timestamp initialDataTimestamp(_initialDataTimestamp.load());
            const bool keepOldBehavior = true;

            try {
                if (keepOldBehavior) {
                    UniqueWiredTigerSession session = _sessionCache->getSession();
                    WT_SESSION* s = session->getSession();
                    invariantWTOK(s->checkpoint(s, nullptr));
                    LOG(4) << "created checkpoint (forced)";
                } else {
                    // Three cases:
                    //
                    // First, initialDataTimestamp is Timestamp(0, 1) -> Take full
                    // checkpoint. This is when there is no consistent view of the data (i.e:
                    // during initial sync).
                    //
                    // Second, stableTimestamp < initialDataTimestamp: Skip checkpoints. The data
                    // on disk is prone to being rolled back. Hold off on checkpoints.  Hope that
                    // the stable timestamp surpasses the data on disk, allowing storage to
                    // persist newer copies to disk.
                    //
                    // Third, stableTimestamp >= initialDataTimestamp: Take stable
                    // checkpoint. Steady state case.
                    if (initialDataTimestamp.asULL() <= 1) {
                        const bool forceCheckpoint = true;
                        const bool stableCheckpoint = false;
                        _sessionCache->waitUntilDurable(forceCheckpoint, stableCheckpoint);
                    } else if (stableTimestamp < initialDataTimestamp) {
                        LOG(1) << "Stable timestamp is behind the initial data timestamp, skipping "
                                  "a checkpoint. StableTimestamp: "
                               << stableTimestamp.toString()
                               << " InitialDataTimestamp: " << initialDataTimestamp.toString();
                    } else {
                        const bool forceCheckpoint = true;
                        const bool stableCheckpoint = true;
                        _sessionCache->waitUntilDurable(forceCheckpoint, stableCheckpoint);
                    }
                }
            } catch (const WriteConflictException&) {
                // Temporary: remove this after WT-3483
                warning() << "Checkpoint encountered a write conflict exception.";
            } catch (const AssertionException& exc) {
                invariant(exc.code() == ErrorCodes::ShutdownInProgress);
            }
        }
        LOG(1) << "stopping " << name() << " thread";
    }

	//WiredTigerKVEngine::supportsRecoverToStableTimestamp中调用
    bool supportsRecoverToStableTimestamp() {
        // Replication is calling this method, however it is not setting the
        // `_initialDataTimestamp` in all necessary cases. This may be removed when replication
        // believes all sets of `_initialDataTimestamp` are correct. See SERVER-30184,
        // SERVER-30185, SERVER-30335.
        const bool keepOldBehavior = true;
        if (keepOldBehavior) {
            return false;
        }

        static const std::uint64_t allowUnstableCheckpointsSentinel =
            static_cast<std::uint64_t>(Timestamp::kAllowUnstableCheckpointsSentinel.asULL());
        const std::uint64_t initialDataTimestamp = _initialDataTimestamp.load();
        // Illegal to be called when the dataset is incomplete.
        invariant(initialDataTimestamp > allowUnstableCheckpointsSentinel);

        // Must return false until `recoverToStableTimestamp` is implemented. See SERVER-29213.
        if (keepOldBehavior) {
            return false;
        }
        return _stableTimestamp.load() > initialDataTimestamp;
    }

	//WiredTigerKVEngine::setStableTimestamp中调用
    void setStableTimestamp(Timestamp stableTimestamp) {
        _stableTimestamp.store(stableTimestamp.asULL());
    }

    void setInitialDataTimestamp(Timestamp initialDataTimestamp) {
        _initialDataTimestamp.store(initialDataTimestamp.asULL());
    }

    void shutdown() {
        _shuttingDown.store(true);
        _condvar.notify_one();
        wait();
    }

private:
    WiredTigerSessionCache* _sessionCache;

    // _mutex/_condvar used to notify when _shuttingDown is flipped.
    stdx::mutex _mutex;
    stdx::condition_variable _condvar;
    AtomicBool _shuttingDown{false};
	//前面的setStableTimestamp设置
    AtomicWord<std::uint64_t> _stableTimestamp; 
	//前面的setInitialDataTimestamp设置
    AtomicWord<std::uint64_t> _initialDataTimestamp;
};

namespace {

class TicketServerParameter : public ServerParameter {
    MONGO_DISALLOW_COPYING(TicketServerParameter);

public:
    TicketServerParameter(TicketHolder* holder, const std::string& name)
		//db.adminCommand( { setParameter : 1, "wiredTigerEngineRuntimeConfig" : "cache_size=2GB" })
		//db.adminCommand( { getParameter : "1", wiredTigerEngineRuntimeConfig : 1  } )
        : ServerParameter(ServerParameterSet::getGlobal(), name, true, true), _holder(holder) {}

    virtual void append(OperationContext* opCtx, BSONObjBuilder& b, const std::string& name) {
        b.append(name, _holder->outof());
    }

    virtual Status set(const BSONElement& newValueElement) {
        if (!newValueElement.isNumber())
            return Status(ErrorCodes::BadValue, str::stream() << name() << " has to be a number");
        return _set(newValueElement.numberInt());
    }

    virtual Status setFromString(const std::string& str) {
        int num = 0;
        Status status = parseNumberFromString(str, &num);
        if (!status.isOK())
            return status;
        return _set(num);
    }

	//上面的set调用
    Status _set(int newNum) { 
        if (newNum <= 0) {
            return Status(ErrorCodes::BadValue, str::stream() << name() << " has to be > 0");
        }

        return _holder->resize(newNum); //TicketHolder::resize
    }

private:
    TicketHolder* _holder; 
};

//生效见WiredTigerKVEngine::WiredTigerKVEngine->Locker::setGlobalThrottling
TicketHolder openWriteTransaction(128); 
//赋值给TicketServerParameter._holder
TicketServerParameter openWriteTransactionParam(&openWriteTransaction,
                                                "wiredTigerConcurrentWriteTransactions");
//生效见WiredTigerKVEngine::WiredTigerKVEngine->Locker::setGlobalThrottling
TicketHolder openReadTransaction(128);
//赋值给TicketServerParameter._holder
TicketServerParameter openReadTransactionParam(&openReadTransaction,
                                               "wiredTigerConcurrentReadTransactions");

stdx::function<bool(StringData)> initRsOplogBackgroundThreadCallback = [](StringData) -> bool {
    fassertFailed(40358);
};
}  // namespace

/*
wiredtiger简单例子:
//error_check(wiredtiger_open(home, NULL, CONN_CONFIG, &conn));

//__conn_open_session
//error_check(conn->open_session(conn, NULL, NULL, &session));

//__session_create	创建table表
//error_check(session->create(
	session, "table:access", "key_format=S,value_format=S"));

//__session_open_cursor  //获取一个cursor通过cursorp返回
//error_check(session->open_cursor(
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

//ServiceContextMongoD::initializeGlobalStorageEngine->WiredTigerFactory::create
//KVStorageEngine._engine为WiredTigerKVEngine
WiredTigerKVEngine::WiredTigerKVEngine(const std::string& canonicalName,
                                       const std::string& path,
                                       ClockSource* cs,
                                       const std::string& extraOpenOptions,
                                       size_t cacheSizeMB,
                                       bool durable,
                                       bool ephemeral,
                                       bool repair,
                                       bool readOnly)
    : _keepDataHistory(serverGlobalParams.enableMajorityReadConcern),
      _eventHandler(WiredTigerUtil::defaultEventHandlers()),
      _clockSource(cs),
      _oplogManager(stdx::make_unique<WiredTigerOplogManager>()),
      _canonicalName(canonicalName),
      //数据目录
      _path(path),
      _sizeStorerSyncTracker(cs, 100000, Seconds(60)),
      //mongod --journal 
      _durable(durable),
      //mongod  --repair 启用
      _ephemeral(ephemeral),
      _readOnly(readOnly) {
    boost::filesystem::path journalPath = path;
    journalPath /= "journal";
    if (_durable) {
        if (!boost::filesystem::exists(journalPath)) {
            try {
                boost::filesystem::create_directory(journalPath);
            } catch (std::exception& e) {
                log() << "error creating journal dir " << journalPath.string() << ' ' << e.what();
                throw;
            }
        }
    }

    _previousCheckedDropsQueued = _clockSource->now();

    std::stringstream ss;
    ss << "create,";
    ss << "cache_size=" << cacheSizeMB << "M,";
    ss << "session_max=20000,";
    ss << "eviction=(threads_min=4,threads_max=4),";
    ss << "config_base=false,";
    ss << "statistics=(fast),";

    // The setting may have a later setting override it if not using the journal.  We make it
    // unconditional here because even nojournal may need this setting if it is a transition
    // from using the journal.
    if (!_readOnly) {
        // If we're readOnly skip all WAL-related settings.
        ss << "log=(enabled=true,archive=true,path=journal,compressor=";
        ss << wiredTigerGlobalOptions.journalCompressor << "),";
        ss << "file_manager=(close_idle_time=100000),";  //~28 hours, will put better fix in 3.1.x
        ss << "statistics_log=(wait=" << wiredTigerGlobalOptions.statisticsLogDelaySecs << "),";
        ss << "verbose=(recovery_progress),";
    }
    ss << WiredTigerCustomizationHooks::get(getGlobalServiceContext())
              ->getTableCreateConfig("system");
    ss << WiredTigerExtensions::get(getGlobalServiceContext())->getOpenExtensionsConfig();
    ss << extraOpenOptions;
    if (_readOnly) {
        invariant(!_durable);
        ss << "readonly=true,";
    }

	//mongod启动没有携带//mongod --journal 
    if (!_durable && !_readOnly) {
        // If we started without the journal, but previously used the journal then open with the
        // WT log enabled to perform any unclean shutdown recovery and then close and reopen in
        // the normal path without the journal.
        //启动mongod没有是能journal功能，但是该目录存在，则直接清除该目录
        if (boost::filesystem::exists(journalPath)) {
            string config = ss.str();
            log() << "Detected WT journal files.  Running recovery from last checkpoint.";
            log() << "journal to nojournal transition config: " << config;
			//_eventHandler记录wiredtiger_open的事件信息
			int ret = wiredtiger_open(path.c_str(), &_eventHandler, config.c_str(), &_conn);
            if (ret == EINVAL) {
                fassertFailedNoTrace(28717);
            } else if (ret != 0) {
                Status s(wtRCToStatus(ret));
                msgasserted(28718, s.reason());
            }
            invariantWTOK(_conn->close(_conn, NULL));
            // After successful recovery, remove the journal directory.
            try {
                boost::filesystem::remove_all(journalPath);
            } catch (std::exception& e) {
                error() << "error removing journal dir " << journalPath.string() << ' ' << e.what();
                throw;
            }
        }
        // This setting overrides the earlier setting because it is later in the config string.
        ////mongod --journal 如果命令行或者配置没有启用journal，则增加该存储引擎参数
        ss << ",log=(enabled=false),";
    }
    string config = ss.str();
    log() << "wiredtiger_open config: " << config;
    _wtOpenConfig = config;

	/*
	2020-01-26T09:51:52.304+0800 I STORAGE  [initandlisten] 
	wiredtiger_open config: create,cache_size=61440M,session_max=20000,eviction=(threads_min=4,threads_max=4),
	config_base=false,statistics=(fast),log=(enabled=true,archive=true,path=journal,compressor=snappy),
	file_manager=(close_idle_time=100000),statistics_log=(wait=0),verbose=(recovery_progress),
	*/
    int ret = wiredtiger_open(path.c_str(), &_eventHandler, config.c_str(), &_conn);
    // Invalid argument (EINVAL) is usually caused by invalid configuration string.
    // We still fassert() but without a stack trace.
    if (ret == EINVAL) {
        fassertFailedNoTrace(28561);
    } else if (ret != 0) {
        Status s(wtRCToStatus(ret));
        msgasserted(28595, s.reason());
    }

	//_sessionCache指向新的new对象，session信息都从这里获取
    _sessionCache.reset(new WiredTigerSessionCache(this)); //_sessionCache指针赋初值

	//启用了journal日志，并且不是repair修数据，则启用WTJournalFlusher线程
    if (_durable && !_ephemeral) {
        _journalFlusher = stdx::make_unique<WiredTigerJournalFlusher>(_sessionCache.get());
        _journalFlusher->go();
    }

	//不是只读节点并且不是repair数据，则启用WTCheckpointThread线程
    if (!_readOnly && !_ephemeral) {
        _checkpointThread = stdx::make_unique<WiredTigerCheckpointThread>(_sessionCache.get());
        _checkpointThread->go();
    }

	//WiredTigerKVEngine::WiredTigerKVEngine中初始化，对应WiredTigerKVEngine._sizeStorerUri="table:sizeStorer"
    _sizeStorerUri = "table:sizeStorer";
    WiredTigerSession session(_conn);
	//修复sizeStorer.wt数据
    if (!_readOnly && repair && _hasUri(session.getSession(), _sizeStorerUri)) {
        log() << "Repairing size cache";
        fassertNoTrace(28577, _salvageIfNeeded(_sizeStorerUri.c_str()));
    }

    const bool sizeStorerLoggingEnabled = !getGlobalReplSettings().usingReplSets();
    _sizeStorer.reset(
		//对sizeStorer.wt内容操作 _sizeStorerUri = "table:sizeStorer";
		//sizeStorer.wt内容  记录各个集合的记录数和集合总字节数，缓存到内存中
        new WiredTigerSizeStorer(_conn, _sizeStorerUri, sizeStorerLoggingEnabled, _readOnly));
	//WiredTigerSizeStorer::fillCache
	_sizeStorer->fillCache();

	//WiredTigerKVEngine::WiredTigerKVEngine->Locker::setGlobalThrottling
    Locker::setGlobalThrottling(&openReadTransaction, &openWriteTransaction);
}


WiredTigerKVEngine::~WiredTigerKVEngine() {
    if (_conn) {
        cleanShutdown();
    }

    _sessionCache.reset(NULL);
}

/*
opush_gQmJGvRW_shard_1:PRIMARY> db.serverStatus().wiredTiger.concurrentTransactions
{
        "write" : {
                "out" : 0,
                "available" : 128,
                "totalTickets" : 128
        },
        "read" : {
                "out" : 1,
                "available" : 127,
                "totalTickets" : 128
        }
}
*/
//db.serverStatus().wiredTiger.concurrentTransactions命令获取
void WiredTigerKVEngine::appendGlobalStats(BSONObjBuilder& b) {
    BSONObjBuilder bb(b.subobjStart("concurrentTransactions"));
    {
        BSONObjBuilder bbb(bb.subobjStart("write"));
        bbb.append("out", openWriteTransaction.used());
        bbb.append("available", openWriteTransaction.available());
        bbb.append("totalTickets", openWriteTransaction.outof());
        bbb.done();
    }
    {
        BSONObjBuilder bbb(bb.subobjStart("read"));
        bbb.append("out", openReadTransaction.used());
        bbb.append("available", openReadTransaction.available());
        bbb.append("totalTickets", openReadTransaction.outof());
        bbb.done();
    }
    bb.done();
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
void WiredTigerKVEngine::cleanShutdown() {
    log() << "WiredTigerKVEngine shutting down";
    if (!_readOnly)
        syncSizeInfo(true);
    if (_conn) {
        // these must be the last things we do before _conn->close();
        if (_journalFlusher)
            _journalFlusher->shutdown();
        if (_checkpointThread)
            _checkpointThread->shutdown();
        _sizeStorer.reset();
        _sessionCache->shuttingDown();

// We want WiredTiger to leak memory for faster shutdown except when we are running tools to
// look for memory leaks.
#if !__has_feature(address_sanitizer)
        bool leak_memory = true;
#else
        bool leak_memory = false;
#endif
        const char* closeConfig = nullptr;

        if (RUNNING_ON_VALGRIND) {
            leak_memory = false;
        }

        if (leak_memory) {
            closeConfig = "leak_memory=true";
        }

        // There are two cases to consider where the server will shutdown before the in-memory FCV
        // state is set. One is when `EncryptionHooks::restartRequired` is true. The other is when
        // the server shuts down because it refuses to acknowledge an FCV value more than one
        // version behind (e.g: 3.6 errors when reading 3.2).
        //
        // In the first case, we ideally do not perform a file format downgrade (but it is
        // acceptable). In the second, the server must downgrade to allow a 3.4 binary to start
        // up. Ideally, our internal FCV value would allow for older values, even if only to
        // immediately shutdown. This would allow downstream logic, such as this method, to make
        // an informed decision.
        const bool needsDowngrade = !_readOnly &&
            serverGlobalParams.featureCompatibility.getVersion() ==
                ServerGlobalParams::FeatureCompatibility::Version::kFullyDowngradedTo34;

        invariantWTOK(_conn->close(_conn, closeConfig));
        _conn = nullptr;

        // If FCV 3.4, enable WT logging on all tables.
        if (needsDowngrade) {
            // Steps for downgrading:
            //
            // 1) Close and reopen WiredTiger. This clears out any leftover cursors that get in
            //    the way of performing the downgrade.
            //
            // 2) Enable WiredTiger logging on all tables.
            //
            // 3) Reconfigure the WiredTiger to release compatibility 2.9. The WiredTiger version
            //    shipped with MongoDB 3.4 will always refuse to start up without this reconfigure
            //    being successful. Doing this last prevents MongoDB running in 3.4 with only some
            //    underlying tables being logged.
            LOG(1) << "Downgrading WiredTiger tables to release compatibility 2.9";
            WT_CONNECTION* conn;
            std::stringstream openConfig;
            openConfig << _wtOpenConfig << ",log=(archive=false)";
            invariantWTOK(
                wiredtiger_open(_path.c_str(), &_eventHandler, openConfig.str().c_str(), &conn));

            WT_SESSION* session;
            conn->open_session(conn, nullptr, "", &session);

            WT_CURSOR* tableCursor;
            invariantWTOK(
                session->open_cursor(session, "metadata:", nullptr, nullptr, &tableCursor));
            while (tableCursor->next(tableCursor) == 0) {
                const char* raw;
                tableCursor->get_key(tableCursor, &raw);
                StringData key(raw);
                size_t idx = key.find(':');
                if (idx == string::npos) {
                    continue;
                }

                StringData type = key.substr(0, idx);
                if (type != "table") {
                    continue;
                }

                uassertStatusOK(WiredTigerUtil::setTableLogging(session, raw, true));
            }

            tableCursor->close(tableCursor);
            session->close(session, nullptr);
            invariantWTOK(conn->reconfigure(conn, "compatibility=(release=2.9)"));
            invariantWTOK(conn->close(conn, closeConfig));
        }
    }
}

//KVDatabaseCatalogEntryBase::renameCollection 修改表名的时候走到这里
//cache中记录的表数据大小，表重命名后，记录数元数据文件sizeStorer.wt也需要对应修改
Status WiredTigerKVEngine::okToRename(OperationContext* opCtx,
                                      StringData fromNS,
                                      StringData toNS,
                                      StringData ident,
                                      const RecordStore* originalRecordStore) const {
    _sizeStorer->storeToCache(
        _uri(ident), originalRecordStore->numRecords(opCtx), originalRecordStore->dataSize(opCtx));
    syncSizeInfo(true);
    return Status::OK();
}

int64_t WiredTigerKVEngine::getIdentSize(OperationContext* opCtx, StringData ident) {
    WiredTigerSession* session = WiredTigerRecoveryUnit::get(opCtx)->getSession();
    return WiredTigerUtil::getIdentSize(session->getSession(), _uri(ident));
}

//修复ident数据
Status WiredTigerKVEngine::repairIdent(OperationContext* opCtx, StringData ident) {
    WiredTigerSession* session = WiredTigerRecoveryUnit::get(opCtx)->getSession();
    string uri = _uri(ident);
    session->closeAllCursors(uri);
    _sessionCache->closeAllCursors(uri);
    if (isEphemeral()) {
        return Status::OK();
    }
    return _salvageIfNeeded(uri.c_str());
}

//修复uri文件数据
Status WiredTigerKVEngine::_salvageIfNeeded(const char* uri) {
    // Using a side session to avoid transactional issues
    WiredTigerSession sessionWrapper(_conn);
    WT_SESSION* session = sessionWrapper.getSession();

    int rc = (session->verify)(session, uri, NULL);
    if (rc == 0) {
        log() << "Verify succeeded on uri " << uri << ". Not salvaging.";
        return Status::OK();
    }

    if (rc == EBUSY) {
        // SERVER-16457: verify and salvage are occasionally failing with EBUSY. For now we
        // lie and return OK to avoid breaking tests. This block should go away when that ticket
        // is resolved.
        error()
            << "Verify on " << uri << " failed with EBUSY. "
            << "This means the collection was being accessed. No repair is necessary unless other "
               "errors are reported.";
        return Status::OK();
    }

    // TODO need to cleanup the sizeStorer cache after salvaging.
    log() << "Verify failed on uri " << uri << ". Running a salvage operation.";
    return wtRCToStatus(session->salvage(session, uri, NULL), "Salvage failed:");
}

int WiredTigerKVEngine::flushAllFiles(OperationContext* opCtx, bool sync) {
    LOG(1) << "WiredTigerKVEngine::flushAllFiles";
    if (_ephemeral) {
        return 0;
    }
    syncSizeInfo(true);
    const bool forceCheckpoint = true;
    // If there's no journal, we must take a full checkpoint.
    const bool stableCheckpoint = _durable;
	//WiredTigerSessionCache::waitUntilDurable
    _sessionCache->waitUntilDurable(forceCheckpoint, stableCheckpoint);

    return 1;
}

Status WiredTigerKVEngine::beginBackup(OperationContext* opCtx) {
    invariant(!_backupSession);

    // The inMemory Storage Engine cannot create a backup cursor.
    if (_ephemeral) {
        return Status::OK();
    }

    // This cursor will be freed by the backupSession being closed as the session is uncached
    auto session = stdx::make_unique<WiredTigerSession>(_conn);
    WT_CURSOR* c = NULL;
    WT_SESSION* s = session->getSession();
    int ret = WT_OP_CHECK(s->open_cursor(s, "backup:", NULL, NULL, &c));
    if (ret != 0) {
        return wtRCToStatus(ret);
    }
    _backupSession = std::move(session);
    return Status::OK();
}

void WiredTigerKVEngine::endBackup(OperationContext* opCtx) {
    _backupSession.reset();
}

//参考http://www.mongoing.com/archives/5476  同步内存中的size到磁盘
//WiredTigerKVEngine::haveDropsQueued(定时检测执行)  WiredTigerKVEngine::flushAllFiles中调用
void WiredTigerKVEngine::syncSizeInfo(bool sync) const {
    if (!_sizeStorer)
        return;

    try {
		//WiredTigerSizeStorer::syncCache
        _sizeStorer->syncCache(sync);
    } catch (const WriteConflictException&) {
        // ignore, we'll try again later.
    }
}

// ServiceContextMongoD::_newOpCtx->KVStorageEngine::newRecoveryUnit中初始化该类
RecoveryUnit* WiredTigerKVEngine::newRecoveryUnit() {
//WiredTigerRecoveryUnit和WiredTigerKVEngine._sessionCache类通过WiredTigerKVEngine::newRecoveryUnit()关联起来
    return new WiredTigerRecoveryUnit(_sessionCache.get()); //_sessionCache在WiredTigerKVEngine::WiredTigerKVEngine初始化
}

//WiredTigerFactory::create中赋值
void WiredTigerKVEngine::setRecordStoreExtraOptions(const std::string& options) {
    _rsOptions = options;
}

//WiredTigerFactory::create中赋值
void WiredTigerKVEngine::setSortedDataInterfaceExtraOptions(const std::string& options) {
    _indexOptions = options;
}

/*

Breakpoint 1, mongo::WiredTigerKVEngine::createGroupedRecordStore (this=0x7fe15cc22680, opCtx=0x7fe16061be00, ns=..., ident=..., options=..., prefix=...) at src/mongo/db/storage/wiredtiger/wiredtiger_kv_engine.cpp:752
752             _canonicalName, ns, options, _rsOptions, prefixed);
(gdb) bt
#0  mongo::WiredTigerKVEngine::createGroupedRecordStore (this=0x7fe15cc22680, opCtx=0x7fe16061be00, ns=..., ident=..., options=..., prefix=...) at src/mongo/db/storage/wiredtiger/wiredtiger_kv_engine.cpp:752
#1  0x00007fe159986696 in mongo::KVDatabaseCatalogEntryBase::createCollection (this=0x7fe15cd695a0, opCtx=0x7fe16061be00, ns=..., options=..., allocateDefaultSpace=true) at src/mongo/db/storage/kv/kv_database_catalog_entry_base.cpp:227
#2  0x00007fe159b6a28b in mongo::DatabaseImpl::createCollection (this=0x7fe15cd6d700, opCtx=0x7fe16061be00, ns=..., options=..., createIdIndex=<optimized out>, idIndex=...) at src/mongo/db/catalog/database_impl.cpp:827
#3  0x00007fe159b649e1 in createCollection (idIndex=..., createDefaultIndexes=true, options=..., ns=..., opCtx=0x7fe16061be00, this=0x7fe15cf812d8) at src/mongo/db/catalog/database.h:298
#4  mongo::userCreateNSImpl (opCtx=0x7fe16061be00, db=0x7fe15cf812d8, ns=..., options=..., parseKind=mongo::CollectionOptions::parseForCommand, createDefaultIndexes=true, idIndex=...) at src/mongo/db/catalog/database_impl.cpp:1128
#5  0x00007fe159b6c12f in std::_Function_handler<mongo::Status (mongo::OperationContext*, mongo::Database*, mongo::StringData, mongo::BSONObj, mongo::CollectionOptions::ParseKind, bool, mongo::BSONObj const&), mongo::Status (*)(mongo::OperationContext*, mongo::Database*, mongo::StringData, mongo::BSONObj, mongo::CollectionOptions::ParseKind, bool, mongo::BSONObj const&)>::_M_invoke(std::_Any_data const&, mongo::OperationContext*&&, mongo::Database*&&, mongo::StringData&&, mongo::BSONObj&&, mongo::CollectionOptions::ParseKind&&, bool&&, mongo::BSONObj const&) (__functor=..., __args#0=<optimized out>, __args#1=<optimized out>, __args#2=<optimized out>, __args#3=<optimized out>, 
    __args#4=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x7fe0e66, DIE 0x8138ea1>, __args#5=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x7fe0e66, DIE 0x8138ea6>, 
    __args#6=...) at /usr/local/include/c++/5.4.0/functional:1857
#6  0x00007fe15ae9fd2e in operator() (__args#6=..., __args#5=true, __args#4=mongo::CollectionOptions::parseForCommand, __args#3=<error reading variable: access outside bounds of object referenced via synthetic pointer>, __args#2=..., 
    __args#1=0x7fe15cf812d8, __args#0=0x7fe16061be00, this=0x7fe15bd73800 <mongo::(anonymous namespace)::userCreateNSImpl>) at /usr/local/include/c++/5.4.0/functional:2267
#7  mongo::userCreateNS (opCtx=<optimized out>, db=<optimized out>, ns=..., options=..., parseKind=mongo::CollectionOptions::parseForCommand, createDefaultIndexes=true, idIndex=...) at src/mongo/db/catalog/database.cpp:90
#8  0x00007fe159aed891 in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fe158e67d80) at src/mongo/db/ops/write_ops_exec.cpp:189
#9  0x00007fe159aedabc in writeConflictRetry<mongo::(anonymous namespace)::makeCollection(mongo::OperationContext*, const mongo::NamespaceString&)::<lambda()> > (
    f=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x69f2164, DIE 0x6b5869d>, ns=..., opStr=..., opCtx=0x7fe16061be00) at src/mongo/db/concurrency/write_conflict_exception.h:91
#10 mongo::(anonymous namespace)::makeCollection (opCtx=opCtx@entry=0x7fe16061be00, ns=...) at src/mongo/db/ops/write_ops_exec.cpp:192
#11 0x00007fe159af242a in operator() (__closure=0x7fe158e67f40) at src/mongo/db/ops/write_ops_exec.cpp:363
#12 insertBatchAndHandleErrors (out=0x7fe158e67f20, lastOpFixer=0x7fe158e67f00, batch=..., wholeOp=..., opCtx=0x7fe16061be00) at src/mongo/db/ops/write_ops_exec.cpp:371
#13 mongo::performInserts (opCtx=opCtx@entry=0x7fe16061be00, wholeOp=...) at src/mongo/db/ops/write_ops_exec.cpp:533
#14 0x00007fe159ad958e in mongo::(anonymous namespace)::CmdInsert::runImpl (this=<optimized out>, opCtx=0x7fe16061be00, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:255
#15 0x00007fe159ad3128 in mongo::(anonymous namespace)::WriteCommand::enhancedRun (this=<optimized out>, opCtx=0x7fe16061be00, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:221
#16 0x00007fe15aa9b66f in mongo::Command::publicRun (this=0x7fe15bd4b3a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7fe16061be00, request=..., result=...) at src/mongo/db/commands.cpp:355
#17 0x00007fe159a17774 in runCommandImpl (startOperationTime=..., replyBuilder=0x7fe1610f99f0, request=..., command=0x7fe15bd4b3a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7fe16061be00)
    at src/mongo/db/service_entry_point_mongod.cpp:506
#18 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7fe16061be00, command=command@entry=0x7fe15bd4b3a0 <mongo::(anonymous namespace)::cmdInsert>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:759
#19 0x00007fe159a182df in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fe158e68400) at src/mongo/db/service_entry_point_mongod.cpp:880
#20 0x00007fe159a182df in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#21 0x00007fe159a19141 in runCommands (message=..., opCtx=0x7fe16061be00) at src/mongo/db/service_entry_point_mongod.cpp:890
#22 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7fe16061be00, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1163
#23 0x00007fe159a25a7a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:421
#24 0x00007fe159a20bbf in mongo::ServiceStateMachine::_runNextInGuard (this=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:498
#25 0x00007fe159a245fe in operator() (__closure=0x7fe15cf5f8e0) at src/mongo/transport/service_state_machine.cpp:539
#26 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#27 0x00007fe15a960b72 in operator() (this=0x7fe158e6a550) at /usr/local/include/c++/5.4.0/functional:2267
#28 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7fe15cf59480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:125
#29 0x00007fe159a1f7bd in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7fe15cf585f0, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:543
#30 0x00007fe159a22151 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7fe15cf585f0, status=...) at src/mongo/transport/service_state_machine.cpp:324
#31 0x00007fe159a22d4b in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:281
#32 0x00007fe159a20c51 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:495
#33 0x00007fe159a245fe in operator() (__closure=0x7fe15cf5fbc0) at src/mongo/transport/service_state_machine.cpp:539
#34 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#35 0x00007fe15a9610d5 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#36 operator() (__closure=0x7fe1610f9810) at src/mongo/transport/service_executor_synchronous.cpp:143
#37 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#38 0x00007fe15aeb0ca4 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#39 mongo::(anonymous namespace)::runFunc (ctx=0x7fe15cf5f9c0) at src/mongo/transport/service_entry_point_utils.cpp:55
*/
/* 调用的地方如下:
KVDatabaseCatalogEntryBase::createCollection
KVStorageEngine::KVStorageEngine
*/ 
//WiredTigerKVEngine::createGroupedRecordStore(数据文件相关 包括元数据文件如_mdb_catalog.wt和普通数据文件)  
//WiredTigerKVEngine::createGroupedSortedDataInterface(索引文件相关)
//调用WT存储引擎的create接口建表
Status WiredTigerKVEngine::createGroupedRecordStore(OperationContext* opCtx,
                                                    StringData ns,
                                                    StringData ident,
                                                    const CollectionOptions& options,
                                                    KVPrefix prefix) {
    _checkIdentPath(ident);
    WiredTigerSession session(_conn);

    const bool prefixed = prefix.isPrefixed();
    StatusWith<std::string> result = WiredTigerRecordStore::generateCreateString(
        _canonicalName, ns, options, _rsOptions, prefixed);
    if (!result.isOK()) {
        return result.getStatus();
    }
    std::string config = result.getValue();

    //ident是类似这种形式：6--4057196034770697536.wt，_uri(ident)返回的是string("table:") + ident.toString();
    string uri = _uri(ident); //uri加上table前缀

	//对应open_session   WiredTigerSessionCache::getSession
	//这里session会和WiredTigerSessionCache._sessions关联起来
    WT_SESSION* s = session.getSession();
	
	// WiredTigerKVEngine::createRecordStore ns: test.test uri: table:test/collection/4--6382651395048738792 
	//config: type=file,memory_page_max=10m,split_pct=90,leaf_value_max=64MB,checksum=on,block_compressor=snappy,
	//,key_format=q,value_format=u,app_metadata=(formatVersion=1),log=(enabled=true)
    LOG(2) << "WiredTigerKVEngine::createGroupedRecordStore ns: " << ns << " uri: " << uri
           << " config: " << config;
	//对应wiredtiger session->create  
    return wtRCToStatus(s->create(s, uri.c_str(), config.c_str()));
}

//建表KVDatabaseCatalogEntryBase::createCollection->WiredTigerKVEngine::getGroupedRecordStore中调用
//KVStorageEngine::KVStorageEngine调用
//获取StandardWiredTigerRecordStore类，获取表对应的RecordStore，对该表的KV操作就通过该返回类实现
std::unique_ptr<RecordStore> WiredTigerKVEngine::getGroupedRecordStore(
    OperationContext* opCtx,
    StringData ns,
    StringData ident,
    const CollectionOptions& options,
    KVPrefix prefix) {

    WiredTigerRecordStore::Params params;
	//表名
    params.ns = ns;
	//存储路径对应uri，也就是一个RecordStore对应一个表，对该RecordStore操作也就是对表得操作
    params.uri = _uri(ident);
    params.engineName = _canonicalName;
    params.isCapped = options.capped;
    params.isEphemeral = _ephemeral;
    params.cappedCallback = nullptr;
    params.sizeStorer = _sizeStorer.get();
    params.isReadOnly = _readOnly;

    params.cappedMaxSize = -1;
    if (options.capped) {
        if (options.cappedSize) {
            params.cappedMaxSize = options.cappedSize;
        } else {
            params.cappedMaxSize = 4096;
        }
    }
    params.cappedMaxDocs = -1;
    if (options.capped && options.cappedMaxDocs)
        params.cappedMaxDocs = options.cappedMaxDocs;

    std::unique_ptr<WiredTigerRecordStore> ret;
    if (prefix == KVPrefix::kNotPrefixed) {
		//默认用这个
        ret = stdx::make_unique<StandardWiredTigerRecordStore>(this, opCtx, params);
    } else {
        ret = stdx::make_unique<PrefixedWiredTigerRecordStore>(this, opCtx, params, prefix);
    }
	//初始化获取当前table中最大RecordId，最大数据量 数据行数，启动startOplogManager OplogBackgroundThread等
    ret->postConstructorInit(opCtx);

    return std::move(ret);
}

//表名
string WiredTigerKVEngine::_uri(StringData ident) const {
    return string("table:") + ident.toString();
}

/*

Breakpoint 2, mongo::WiredTigerKVEngine::createGroupedSortedDataInterface (this=0x7fe15cc22680, opCtx=0x7fe16061be00, ident=..., desc=0x7fe1611a0180, prefix=...) at src/mongo/db/storage/wiredtiger/wiredtiger_kv_engine.cpp:833
833         if (collection) {
(gdb) bt
#0  mongo::WiredTigerKVEngine::createGroupedSortedDataInterface (this=0x7fe15cc22680, opCtx=0x7fe16061be00, ident=..., desc=0x7fe1611a0180, prefix=...) at src/mongo/db/storage/wiredtiger/wiredtiger_kv_engine.cpp:833
#1  0x00007fe159995dfa in mongo::KVCollectionCatalogEntry::prepareForIndexBuild (this=0x7fe1610deb80, opCtx=0x7fe16061be00, spec=0x7fe1611a0180) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:212
#2  0x00007fe159b73cab in mongo::IndexCatalogImpl::IndexBuildBlock::init (this=this@entry=0x7fe158e673e0) at src/mongo/db/catalog/index_catalog_impl.cpp:418
#3  0x00007fe159b75063 in mongo::IndexCatalogImpl::createIndexOnEmptyCollection (this=0x7fe15cd977e0, opCtx=0x7fe16061be00, spec=...) at src/mongo/db/catalog/index_catalog_impl.cpp:367
#4  0x00007fe159b6ab07 in createIndexOnEmptyCollection (spec=<error reading variable: access outside bounds of object referenced via synthetic pointer>, opCtx=0x7fe16061be00, this=<optimized out>)
    at src/mongo/db/catalog/index_catalog.h:454
#5  mongo::DatabaseImpl::createCollection (this=<optimized out>, opCtx=0x7fe16061be00, ns=..., options=..., createIdIndex=<optimized out>, idIndex=...) at src/mongo/db/catalog/database_impl.cpp:849
#6  0x00007fe159b649e1 in createCollection (idIndex=..., createDefaultIndexes=true, options=..., ns=..., opCtx=0x7fe16061be00, this=0x7fe15cf812d8) at src/mongo/db/catalog/database.h:298
#7  mongo::userCreateNSImpl (opCtx=0x7fe16061be00, db=0x7fe15cf812d8, ns=..., options=..., parseKind=mongo::CollectionOptions::parseForCommand, createDefaultIndexes=true, idIndex=...) at src/mongo/db/catalog/database_impl.cpp:1128
#8  0x00007fe159b6c12f in std::_Function_handler<mongo::Status (mongo::OperationContext*, mongo::Database*, mongo::StringData, mongo::BSONObj, mongo::CollectionOptions::ParseKind, bool, mongo::BSONObj const&), mongo::Status (*)(mongo::OperationContext*, mongo::Database*, mongo::StringData, mongo::BSONObj, mongo::CollectionOptions::ParseKind, bool, mongo::BSONObj const&)>::_M_invoke(std::_Any_data const&, mongo::OperationContext*&&, mongo::Database*&&, mongo::StringData&&, mongo::BSONObj&&, mongo::CollectionOptions::ParseKind&&, bool&&, mongo::BSONObj const&) (__functor=..., __args#0=<optimized out>, __args#1=<optimized out>, __args#2=<optimized out>, __args#3=<optimized out>, 
    __args#4=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x7fe0e66, DIE 0x8138ea1>, __args#5=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x7fe0e66, DIE 0x8138ea6>, 
    __args#6=...) at /usr/local/include/c++/5.4.0/functional:1857
#9  0x00007fe15ae9fd2e in operator() (__args#6=..., __args#5=true, __args#4=mongo::CollectionOptions::parseForCommand, __args#3=<error reading variable: access outside bounds of object referenced via synthetic pointer>, __args#2=..., 
    __args#1=0x7fe15cf812d8, __args#0=0x7fe16061be00, this=0x7fe15bd73800 <mongo::(anonymous namespace)::userCreateNSImpl>) at /usr/local/include/c++/5.4.0/functional:2267
#10 mongo::userCreateNS (opCtx=<optimized out>, db=<optimized out>, ns=..., options=..., parseKind=mongo::CollectionOptions::parseForCommand, createDefaultIndexes=true, idIndex=...) at src/mongo/db/catalog/database.cpp:90
#11 0x00007fe159aed891 in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fe158e67d80) at src/mongo/db/ops/write_ops_exec.cpp:189
#12 0x00007fe159aedabc in writeConflictRetry<mongo::(anonymous namespace)::makeCollection(mongo::OperationContext*, const mongo::NamespaceString&)::<lambda()> > (
    f=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x69f2164, DIE 0x6b5869d>, ns=..., opStr=..., opCtx=0x7fe16061be00) at src/mongo/db/concurrency/write_conflict_exception.h:91
#13 mongo::(anonymous namespace)::makeCollection (opCtx=opCtx@entry=0x7fe16061be00, ns=...) at src/mongo/db/ops/write_ops_exec.cpp:192
#14 0x00007fe159af242a in operator() (__closure=0x7fe158e67f40) at src/mongo/db/ops/write_ops_exec.cpp:363
#15 insertBatchAndHandleErrors (out=0x7fe158e67f20, lastOpFixer=0x7fe158e67f00, batch=..., wholeOp=..., opCtx=0x7fe16061be00) at src/mongo/db/ops/write_ops_exec.cpp:371
#16 mongo::performInserts (opCtx=opCtx@entry=0x7fe16061be00, wholeOp=...) at src/mongo/db/ops/write_ops_exec.cpp:533
#17 0x00007fe159ad958e in mongo::(anonymous namespace)::CmdInsert::runImpl (this=<optimized out>, opCtx=0x7fe16061be00, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:255
#18 0x00007fe159ad3128 in mongo::(anonymous namespace)::WriteCommand::enhancedRun (this=<optimized out>, opCtx=0x7fe16061be00, request=..., result=...) at src/mongo/db/commands/write_commands/write_commands.cpp:221
#19 0x00007fe15aa9b66f in mongo::Command::publicRun (this=0x7fe15bd4b3a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7fe16061be00, request=..., result=...) at src/mongo/db/commands.cpp:355
#20 0x00007fe159a17774 in runCommandImpl (startOperationTime=..., replyBuilder=0x7fe1610f99f0, request=..., command=0x7fe15bd4b3a0 <mongo::(anonymous namespace)::cmdInsert>, opCtx=0x7fe16061be00)
    at src/mongo/db/service_entry_point_mongod.cpp:506
#21 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7fe16061be00, command=command@entry=0x7fe15bd4b3a0 <mongo::(anonymous namespace)::cmdInsert>, request=..., replyBuilder=<optimized out>)
    at src/mongo/db/service_entry_point_mongod.cpp:759
#22 0x00007fe159a182df in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fe158e68400) at src/mongo/db/service_entry_point_mongod.cpp:880
#23 0x00007fe159a182df in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#24 0x00007fe159a19141 in runCommands (message=..., opCtx=0x7fe16061be00) at src/mongo/db/service_entry_point_mongod.cpp:890
#25 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7fe16061be00, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1163
#26 0x00007fe159a25a7a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:421
#27 0x00007fe159a20bbf in mongo::ServiceStateMachine::_runNextInGuard (this=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:498
#28 0x00007fe159a245fe in operator() (__closure=0x7fe15cf5f8e0) at src/mongo/transport/service_state_machine.cpp:539
#29 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#30 0x00007fe15a960b72 in operator() (this=0x7fe158e6a550) at /usr/local/include/c++/5.4.0/functional:2267
#31 mongo::transport::ServiceExecutorSynchronous::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7fe15cf59480, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_synchronous.cpp:125
#32 0x00007fe159a1f7bd in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7fe15cf585f0, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:543
#33 0x00007fe159a22151 in mongo::ServiceStateMachine::_sourceCallback (this=this@entry=0x7fe15cf585f0, status=...) at src/mongo/transport/service_state_machine.cpp:324
#34 0x00007fe159a22d4b in mongo::ServiceStateMachine::_sourceMessage (this=this@entry=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:281
#35 0x00007fe159a20c51 in mongo::ServiceStateMachine::_runNextInGuard (this=0x7fe15cf585f0, guard=...) at src/mongo/transport/service_state_machine.cpp:495
#36 0x00007fe159a245fe in operator() (__closure=0x7fe15cf5fbc0) at src/mongo/transport/service_state_machine.cpp:539
#37 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#38 0x00007fe15a9610d5 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#39 operator() (__closure=0x7fe1610f9810) at src/mongo/transport/service_executor_synchronous.cpp:143
#40 std::_Function_handler<void(), mongo::transport::ServiceExecutorSynchronous::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> >::_M_invoke(const std::_Any_data &) (
    __functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#41 0x00007fe15aeb0ca4 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#42 mongo::(anonymous namespace)::runFunc (ctx=0x7fe15cf5f9c0) at src/mongo/transport/service_entry_point_utils.cpp:55
#43 0x00007fe157b85e25 in start_thread () from /lib64/libpthread.so.0
#44 0x00007fe1578b334d in clone () from /lib64/libc.so.6
*/
//WiredTigerKVEngine::createGroupedRecordStore(数据文件相关 包括元数据文件如_mdb_catalog.wt和普通数据文件) 
//WiredTigerKVEngine::createGroupedSortedDataInterface->WiredTigerIndex::Create(索引文件相关)创建wiredtiger索引文件

//DatabaseImpl::createCollection->IndexCatalogImpl::createIndexOnEmptyCollection->IndexCatalogImpl::IndexBuildBlock::init
//->KVCollectionCatalogEntry::prepareForIndexBuild中执行

//KVCollectionCatalogEntry::prepareForIndexBuild调用，创建索引对应wt session信息，底层建索引表
Status WiredTigerKVEngine::createGroupedSortedDataInterface(OperationContext* opCtx,
                                                            StringData ident,
                                                            const IndexDescriptor* desc,
                                                            KVPrefix prefix) {
    _checkIdentPath(ident);

    std::string collIndexOptions;
    const Collection* collection = desc->getCollection();

    // Treat 'collIndexOptions' as an empty string when the collection member of 'desc' is NULL in
    // order to allow for unit testing WiredTigerKVEngine::createSortedDataInterface().
    if (collection) {
        const CollectionCatalogEntry* cce = collection->getCatalogEntry();
        const CollectionOptions collOptions = cce->getCollectionOptions(opCtx);

        if (!collOptions.indexOptionDefaults["storageEngine"].eoo()) {
            BSONObj storageEngineOptions = collOptions.indexOptionDefaults["storageEngine"].Obj();
            collIndexOptions =
                dps::extractElementAtPath(storageEngineOptions, _canonicalName + ".configString")
                    .valuestrsafe();
        }
    }
	
	/*
	2018-09-25T17:06:01.582+0800 D STORAGE	[conn1] index create string: type=file,internal_page_max=16k,
	leaf_page_max=16k,checksum=on,prefix_compression=true,block_compressor=,,,,key_format=u,value_format=u,
	app_metadata=(formatVersion=8,infoObj={ "v" : 2, "key" : { "_id" : 1 }, "name" : "_id_", "ns" : "test.test" }),
	log=(enabled=true)
	*/ 
	//获取建表的字符串信息
    StatusWith<std::string> result = WiredTigerIndex::generateCreateString(
        _canonicalName, _indexOptions, collIndexOptions, *desc, prefix.isPrefixed());
    if (!result.isOK()) {
        return result.getStatus();
    }

    std::string config = result.getValue();

	/*
	2018-09-25T17:06:01.582+0800 D STORAGE	[conn1] WiredTigerKVEngine::createSortedDataInterface ns: test.test 
	ident: test/index/5--6382651395048738792 config: type=file,internal_page_max=16k,leaf_page_max=16k,checksum=on,
	prefix_compression=true,block_compressor=,,,,key_format=u,value_format=u,app_metadata=(formatVersion=8,infoObj=
	{ "v" : 2, "key" : { "_id" : 1 }, "name" : "_id_", "ns" : "test.test" }),log=(enabled=true)
	*/
    LOG(2) << "WiredTigerKVEngine::createSortedDataInterface ns: " << collection->ns()
           << " ident: " << ident << " config: " << config;
	//调用存储引擎接口WiredTigerIndex::Create创建对应的索引文件 
    return wtRCToStatus(WiredTigerIndex::Create(opCtx, _uri(ident), config));
}

//唯一索引和普通索引对应SortedDataInterface
SortedDataInterface* WiredTigerKVEngine::getGroupedSortedDataInterface(OperationContext* opCtx,
                                                                       StringData ident,
                                                                       const IndexDescriptor* desc,
                                                                       KVPrefix prefix) {
    if (desc->unique())
        return new WiredTigerIndexUnique(opCtx, _uri(ident), desc, prefix, _readOnly);
    return new WiredTigerIndexStandard(opCtx, _uri(ident), desc, prefix, _readOnly);
}

/*
KVCollectionCatalogEntry::RemoveIndexChange
KVCollectionCatalogEntry::AddIndexChange
class KVDatabaseCatalogEntryBase::RemoveCollectionChange
KVStorageEngine::reconcileCatalogAndIdents
*/

////KVDatabaseCatalogEntryBase::commit->WiredTigerKVEngine::dropIdent删表中调用，真正的表删除
//KVCollectionCatalogEntry::RemoveIndexChange::commit()->WiredTigerKVEngine::dropIdent 删索引，中调用，真正删除索引在这里
Status WiredTigerKVEngine::dropIdent(OperationContext* opCtx, StringData ident) {
    string uri = _uri(ident);

	//WiredTigerRecoveryUnit* get
    WiredTigerRecoveryUnit* ru = WiredTigerRecoveryUnit::get(opCtx);
	
    ru->getSessionNoTxn()->closeAllCursors(uri);
	//WiredTigerSessionCache::closeAllCursors
    _sessionCache->closeAllCursors(uri);

    WiredTigerSession session(_conn);

    int ret = session.getSession()->drop(
        session.getSession(), uri.c_str(), "force,checkpoint_wait=false");
    LOG(1) << "WT drop of  " << uri << " res " << ret;

    if (ret == 0) {
        // yay, it worked
        return Status::OK();
    }

    if (ret == EBUSY) {
        // this is expected, queue it up
        {
            stdx::lock_guard<stdx::mutex> lk(_identToDropMutex);
            _identToDrop.push_front(uri);
        }
        _sessionCache->closeCursorsForQueuedDrops(); //WiredTigerSessionCache::closeCursorsForQueuedDrops
        return Status::OK();
    }

    invariantWTOK(ret);
    return Status::OK();
}

//把cache中和_identToDrop匹配的cursor找出来，放入到toDrop中，在后面的
//WiredTigerSession::closeCursorsForQueuedDrops中调用，然后释放这些cursor
std::list<WiredTigerCachedCursor> WiredTigerKVEngine::filterCursorsWithQueuedDrops(
    std::list<WiredTigerCachedCursor>* cache) {
    std::list<WiredTigerCachedCursor> toDrop;

    stdx::lock_guard<stdx::mutex> lk(_identToDropMutex); //自解锁lock_guard
    if (_identToDrop.empty())
        return toDrop;

    for (auto i = cache->begin(); i != cache->end();) {
        if (!i->_cursor ||
            std::find(_identToDrop.begin(), _identToDrop.end(), std::string(i->_cursor->uri)) ==
                _identToDrop.end()) {
            ++i;
            continue;
        }
        toDrop.push_back(*i);
        i = cache->erase(i);
    }

    return toDrop;
}

//WiredTigerSessionCache::releaseSession中调用
bool WiredTigerKVEngine::haveDropsQueued() const {
    Date_t now = _clockSource->now();
    Milliseconds delta = now - _previousCheckedDropsQueued;
	
    if (!_readOnly && _sizeStorerSyncTracker.intervalHasElapsed()) { //定时时间到
        _sizeStorerSyncTracker.resetLastTime();
		//定时时间到，把sizeInfo写入WT
        syncSizeInfo(false);
    }

    // We only want to check the queue max once per second or we'll thrash
    if (delta < Milliseconds(1000)) //1s钟检查一次
        return false;

    _previousCheckedDropsQueued = now;

    // Don't wait for the mutex: if we can't get it, report that no drops are queued.
    stdx::unique_lock<stdx::mutex> lk(_identToDropMutex, stdx::defer_lock);
    return lk.try_lock() && !_identToDrop.empty();
}

//WiredTigerSessionCache::releaseSession中调用
void WiredTigerKVEngine::dropSomeQueuedIdents() {
    int numInQueue;

    WiredTigerSession session(_conn);

    {
        stdx::lock_guard<stdx::mutex> lk(_identToDropMutex);
        numInQueue = _identToDrop.size();
    }

    int numToDelete = 10;
    int tenPercentQueue = numInQueue * 0.1;
    if (tenPercentQueue > 10)
        numToDelete = tenPercentQueue;

    LOG(1) << "WT Queue is: " << numInQueue << " attempting to drop: " << numToDelete << " tables";
    for (int i = 0; i < numToDelete; i++) {
        string uri;
        {
            stdx::lock_guard<stdx::mutex> lk(_identToDropMutex);
            if (_identToDrop.empty())
                break;
            uri = _identToDrop.front();
            _identToDrop.pop_front();
        }
        int ret = session.getSession()->drop(
            session.getSession(), uri.c_str(), "force,checkpoint_wait=false");
        LOG(1) << "WT queued drop of  " << uri << " res " << ret;

        if (ret == EBUSY) {
            stdx::lock_guard<stdx::mutex> lk(_identToDropMutex);
            _identToDrop.push_back(uri);
        } else {
            invariantWTOK(ret);
        }
    }
}

bool WiredTigerKVEngine::supportsDocLocking() const {
    return true;
}

bool WiredTigerKVEngine::supportsDirectoryPerDB() const {
    return true;
}

bool WiredTigerKVEngine::hasIdent(OperationContext* opCtx, StringData ident) const {
    return _hasUri(WiredTigerRecoveryUnit::get(opCtx)->getSession()->getSession(), _uri(ident));
}

//检查uri对应文件目录是否存在
bool WiredTigerKVEngine::_hasUri(WT_SESSION* session, const std::string& uri) const {
    // can't use WiredTigerCursor since this is called from constructor.
    WT_CURSOR* c = NULL;
    int ret = session->open_cursor(session, "metadata:", NULL, NULL, &c);
    if (ret == ENOENT)
        return false;
    invariantWTOK(ret);
    ON_BLOCK_EXIT(c->close, c);

    c->set_key(c, uri.c_str());
    return c->search(c) == 0;
}

/*
对MongoDB层可见的所有数据表，在_mdb_catalog表中维护了MongoDB需要的元数据，同样在WiredTiger层中，
会有一份对应的WiredTiger需要的元数据维护在WiredTiger.wt表中。因此，事实上这里有两份数据表的列表，
并且在某些情况下可能会存在不一致，比如，异常宕机的场景。因此MongoDB在启动过程中，会对这两份数据
进行一致性检查，如果是异常宕机启动过程，会以WiredTiger.wt表中的数据为准，对_mdb_catalog表中的记录
进行修正。这个过程会需要遍历WiredTiger.wt表得到所有数据表的列表。 

综上，可以看到，在MongoDB启动过程中，有多处涉及到需要从WiredTiger.wt表中读取数据表的元数据。
对这种需求，WiredTiger专门提供了一类特殊的『metadata』类型的cursor。
*/
//KVStorageEngine::reconcileCatalogAndIdents调用
//KVStorageEngine::reconcileCatalogAndIdents中获取所有得ident，然后后元数据_mdb_catalog.wt中得内容做比较

//从WiredTiger.wt元数据文件中读取库表信息等

//WiredTigerKVEngine::getAllIdents和KVCatalog::getAllIdents区别：
// 1. WiredTigerKVEngine::getAllIdents对应WiredTiger.wt元数据文件，由wiredtiger存储引擎自己维护
// 2. KVCatalog::getAllIdents对应_mdb_catalog.wt，由mongodb server层storage模块维护
// 3. 这两个元数据相比较，冲突的时候collection以_mdb_catalog.wt为准，该表下面的索引以WiredTiger.wt为准
//	  参考KVStorageEngine::reconcileCatalogAndIdents

std::vector<std::string> WiredTigerKVEngine::getAllIdents(OperationContext* opCtx) const {
    std::vector<std::string> all;
	//也就是对WiredTiger.wt得操作
    WiredTigerCursor cursor("metadata:", WiredTigerSession::kMetadataTableId, false, opCtx);
    WT_CURSOR* c = cursor.get();
    if (!c)
        return all;

    while (c->next(c) == 0) {
        const char* raw;
        c->get_key(c, &raw);
        StringData key(raw);
        size_t idx = key.find(':');
        if (idx == string::npos)
            continue;
        StringData type = key.substr(0, idx);
        if (type != "table")
            continue;

        StringData ident = key.substr(idx + 1);
        if (ident == "sizeStorer")
            continue;

        all.push_back(ident.toString());
    }

    return all;
}

int WiredTigerKVEngine::reconfigure(const char* str) {
    return _conn->reconfigure(_conn, str);
}

/*
2018-09-27T11:41:42.574+0800 D STORAGE  [conn1] creating subdirectory: test
2018-09-27T11:41:42.575+0800 D STORAGE  [conn1] creating subdirectory: test/collection
*/
//创建目录，根据ident创建  WiredTigerKVEngine::createGroupedRecordStore  WiredTigerKVEngine::createGroupedSortedDataInterface
//创建ident相关数据或者索引文件目录
void WiredTigerKVEngine::_checkIdentPath(StringData ident) {
    size_t start = 0;
    size_t idx;
    while ((idx = ident.find('/', start)) != string::npos) {
        StringData dir = ident.substr(0, idx);

        boost::filesystem::path subdir = _path;
        subdir /= dir.toString();
        if (!boost::filesystem::exists(subdir)) {
            LOG(1) << "creating subdirectory: " << dir;
            try {
                boost::filesystem::create_directory(subdir);
            } catch (const std::exception& e) {
                error() << "error creating path " << subdir.string() << ' ' << e.what();
                throw;
            }
        }

        start = idx + 1;
    }
}

//KVStorageEngine::setJournalListener
void WiredTigerKVEngine::setJournalListener(JournalListener* jl) {
    return _sessionCache->setJournalListener(jl); //WiredTigerSessionCache::setJournalListener
}

//SetInitRsOplogBackgroundThreadCallback中初始化为initRsOplogBackgroundThread
void WiredTigerKVEngine::setInitRsOplogBackgroundThreadCallback(
    stdx::function<bool(StringData)> cb) {
    initRsOplogBackgroundThreadCallback = std::move(cb);
}

//上面的函数中对initRsOplogBackgroundThreadCallback赋值，这里运行
//WiredTigerRecordStore::postConstructorInit中调用
bool WiredTigerKVEngine::initRsOplogBackgroundThread(StringData ns) {
    return initRsOplogBackgroundThreadCallback(ns);
}

//InitialSyncer::_setUp_inlock->KVStorageEngine::setStableTimestamp->WiredTigerKVEngine::setStableTimestamp
//ReplicationCoordinatorImpl::_setStableTimestampForStorage_inlock->KVStorageEngine::setStableTimestamp->WiredTigerKVEngine::setStableTimestamp
//KVStorageEngine::setStableTimestamp
/*
引擎层 Rollback 与 stable timestamp
在 3.x 版本里，MongoDB 复制集的回滚动作是在 Server 层面完成，但节点需要回滚时，会根据要回滚的 oplog 
不断应用相反的操作，或从回滚源上读取最新的版本，整个回滚操作效率很低。

4.0 版本实现了存储引擎层的回滚机制，当复制集节点需要回滚时，直接调用 WiredTiger 接口，将数据回滚到
某个稳定版本（实际上就是一个 Checkpoint），这个稳定版本则依赖于 stable timestamp。WiredTiger 会确保 
stable timestamp 之后的数据不会写到 Checkpoint里，MongoDB 根据复制集的同步状态，当数据已经同步到大
多数节点时（Majority commited），会更新 stable timestamp，因为这些数据已经提交到大多数节点了，一定
不会发生 ROLLBACK，这个时间戳之前的数据就都可以写到 Checkpoint 里了。

MongoDB 需要确保频繁（及时）的更新 stable timestamp，否则影响 WT Checkpoint 行为，导致很多内存无法释放。
例如主备延时很大，导致数据一直没有被同步到大多数节点，这时主上 stable timestamp 就无法更新，内存不断积
累就可能把 cache 撑满。
参考https://mongoing.com/%3Fp%3D6084
*/
void WiredTigerKVEngine::setStableTimestamp(Timestamp stableTimestamp) {
    const bool keepOldBehavior = true;
    // Communicate to WiredTiger what the "stable timestamp" is. Timestamp-aware checkpoints will
    // only persist to disk transactions committed with a timestamp earlier than the "stable
    // timestamp".
    //
    // After passing the "stable timestamp" to WiredTiger, communicate it to the
    // `CheckpointThread`. It's not obvious a stale stable timestamp in the `CheckpointThread` is
    // safe. Consider the following arguments:
    //
    // Setting the "stable timestamp" is only meaningful when the "initial data timestamp" is real
    // (i.e: not `kAllowUnstableCheckpointsSentinel`). In this normal case, the `stableTimestamp`
    // input must be greater than the current value. The only effect this can have in the
    // `CheckpointThread` is to transition it from a state of not taking any checkpoints, to
    // taking "stable checkpoints". In the transitioning case, it's imperative for the "stable
    // timestamp" to have first been communicated to WiredTiger.
    if (!keepOldBehavior) {
        std::string conf = "stable_timestamp=" + stableTimestamp.toString();
        _conn->set_timestamp(_conn, conf.c_str());
    }
    if (_checkpointThread) {
		//WiredTigerCheckpointThread::setStableTimestamp
        _checkpointThread->setStableTimestamp(stableTimestamp);
    }

    if (_keepDataHistory) {
        // If `_keepDataHistory` is false, the OplogManager is responsible for setting the
        // `oldest_timestamp`.
        //
        // Communicate to WiredTiger that it can clean up timestamp data earlier than the
        // timestamp provided.  No future queries will need point-in-time reads at a timestamp
        // prior to the one provided here.
        advanceOldestTimestamp(stableTimestamp);
    }
}

//WiredTigerRecordStore::cappedTruncateAfter中调用
////KVStorageEngine::setOldestTimestamp
void WiredTigerKVEngine::setOldestTimestamp(Timestamp oldestTimestamp) {
    invariant(oldestTimestamp != Timestamp::min());

    char commitTSConfigString["force=true,oldest_timestamp=,commit_timestamp="_sd.size() +
                              (2 * 8 * 2) /* 8 hexadecimal characters */ + 1 /* trailing null */];
    auto size = std::snprintf(commitTSConfigString,
                              sizeof(commitTSConfigString),
                              "force=true,oldest_timestamp=%llx,commit_timestamp=%llx",
                              oldestTimestamp.asULL(),
                              oldestTimestamp.asULL());
    if (size < 0) {
        int e = errno;
        error() << "error snprintf " << errnoWithDescription(e);
        fassertFailedNoTrace(40662);
    }

    invariant(static_cast<std::size_t>(size) < sizeof(commitTSConfigString));
    invariantWTOK(_conn->set_timestamp(_conn, commitTSConfigString));

    _oplogManager->setOplogReadTimestamp(oldestTimestamp);
    _previousSetOldestTimestamp = oldestTimestamp;
    LOG(1) << "Forced a new oldest_timestamp. Value: " << oldestTimestamp;
}

//KVStorageEngine::advanceOldestTimestamp
void WiredTigerKVEngine::advanceOldestTimestamp(Timestamp oldestTimestamp) {
    if (oldestTimestamp == Timestamp()) {
        // No oldestTimestamp to set, yet.
        return;
    }

    {
        stdx::unique_lock<stdx::mutex> lock(_oplogManagerMutex);
        if (!_oplogManager) {
            // No oplog yet, so don't bother setting oldest_timestamp.
            return;
        }
        auto oplogReadTimestamp = _oplogManager->getOplogReadTimestamp();
        if (oplogReadTimestamp < oldestTimestamp.asULL()) {
            // For one node replica sets, the commit point might race ahead of the oplog read
            // timestamp.
            oldestTimestamp = Timestamp(oplogReadTimestamp);
            if (_previousSetOldestTimestamp > oldestTimestamp) {
                // Do not go backwards.
                return;
            }
        }
    }

    // Lag the oldest_timestamp by one timestamp set, to give a bit more history.
    auto timestampToSet = _previousSetOldestTimestamp;
    _previousSetOldestTimestamp = oldestTimestamp;
    if (timestampToSet == Timestamp()) {
        // Nothing to set yet.
        return;
    }

    char oldestTSConfigString["oldest_timestamp="_sd.size() + (8 * 2) /* 16 hexadecimal digits */ +
                              1 /* trailing null */];
    auto size = std::snprintf(oldestTSConfigString,
                              sizeof(oldestTSConfigString),
                              "oldest_timestamp=%llx",
                              timestampToSet.asULL());
    if (size < 0) {
        int e = errno;
        error() << "error snprintf " << errnoWithDescription(e);
        fassertFailedNoTrace(40661);
    }
    invariant(static_cast<std::size_t>(size) < sizeof(oldestTSConfigString));
    invariantWTOK(_conn->set_timestamp(_conn, oldestTSConfigString));
    LOG(2) << "oldest_timestamp set to " << timestampToSet;
}

//KVStorageEngine::setInitialDataTimestamp
void WiredTigerKVEngine::setInitialDataTimestamp(Timestamp initialDataTimestamp) {
    if (_checkpointThread) {
        _checkpointThread->setInitialDataTimestamp(initialDataTimestamp);
    }
}

bool WiredTigerKVEngine::supportsRecoverToStableTimestamp() const {
    if (_ephemeral) {
        return false;
    }

    return _checkpointThread->supportsRecoverToStableTimestamp();
}

/*
MongoDB 要支持 majority 的 readConcern 级别， 必须设置 replication.enableMajorityReadConcern 参数， 加上这个参数
后， MongoDB 会起一个单独的snapshot 线程， 会周期性的对当前的数据集进行snapshot， 并记录 snapshot 时最新
oplog的时间戳， 得到一个映射表。参考<<MONGODB原理与实战>> readConcern 实现原理章节
*/
//WiredTigerRecordStore::postConstructorInit调用
void WiredTigerKVEngine::startOplogManager(OperationContext* opCtx,
                                           const std::string& uri,
                                           WiredTigerRecordStore* oplogRecordStore) {
    stdx::lock_guard<stdx::mutex> lock(_oplogManagerMutex);
    if (_oplogManagerCount == 0) {
        // If we don't want to keep a long history of data changes, have the OplogManager thread
        // update the oldest timestamp with the "all committed" timestamp, i.e: the latest time at
        // which there are no holes. 
        //WiredTigerOplogManager::start
        _oplogManager->start(opCtx, uri, oplogRecordStore, !_keepDataHistory); //_keepDataHistory参考 WiredTigerKVEngine::WiredTigerKVEngine
    }
    _oplogManagerCount++;
}

void WiredTigerKVEngine::haltOplogManager() {
    stdx::unique_lock<stdx::mutex> lock(_oplogManagerMutex);
    invariant(_oplogManagerCount > 0);
    _oplogManagerCount--;
    if (_oplogManagerCount == 0) {
        // Destructor may lock the mutex, so we must unlock here.
        // Oplog managers only destruct at shutdown or test exit, so it is safe to unlock here.
        lock.unlock();
        _oplogManager->halt(); //WiredTigerOplogManager::halt
    }
}

void WiredTigerKVEngine::replicationBatchIsComplete() const {
    _oplogManager->triggerJournalFlush(); //WiredTigerOplogManager::triggerJournalFlush
}

}  // namespace mongo
