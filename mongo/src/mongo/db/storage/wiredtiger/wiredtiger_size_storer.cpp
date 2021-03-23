// wiredtiger_size_storer.cpp

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

#include <wiredtiger.h>

#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/service_context.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_customization_hooks.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_record_store.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_session_cache.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_size_storer.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_util.h"
#include "mongo/stdx/thread.h"
#include "mongo/util/log.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

using std::string;

namespace {
int MAGIC = 123123;
}

/* 
注意一定要使用官方版本wiredtiger，不要使用我这里分析得：
./wt -v -h /home/yangyazhou/backup2  -C "extensions=[./ext/compressors/snappy/.libs/libwiredtiger_snappy.so]"   dump table:sizeStorer
-h指定数据目录加-j表示json打印：
./wt -v -h /home/yangyazhou/backup2  -C "extensions=[./ext/compressors/snappy/.libs/libwiredtiger_snappy.so]"   dump table:sizeStorer

sizeStorer.wt内容  记录各个集合的记录数和集合总字节数
[root@bogon db]# wt -C "extensions=[/usr/local/lib/libwiredtiger_snappy.so]"  -h . dump table:sizeStorer
WiredTiger Dump (WiredTiger Version 3.0.0)
Format=print
Header
table:sizeStorer
access_pattern_hint=none,allocation_size=4KB,app_metadata=,assert=(commit_timestamp=none,read_timestamp=none),block_allocation=best,block_compressor=,cache_resident=false,checksum=uncompressed,colgroups=,collator=,columns=,dictionary=0,encryption=(keyid=,name=),exclusive=false,extractor=,format=btree,huffman_key=,huffman_value=,ignore_in_memory_cache_size=false,immutable=false,internal_item_max=0,internal_key_max=0,internal_key_truncate=true,internal_page_max=4KB,key_format=u,key_gap=10,leaf_item_max=0,leaf_key_max=0,leaf_page_max=32KB,leaf_value_max=0,log=(enabled=true),lsm=(auto_throttle=true,bloom=true,bloom_bit_count=16,bloom_config=,bloom_hash_count=8,bloom_oldest=false,chunk_count_limit=0,chunk_max=5GB,chunk_size=10MB,merge_max=15,merge_min=0),memory_page_max=5MB,os_cache_dirty_max=0,os_cache_max=0,prefix_compression=false,prefix_compression_min=4,source="file:sizeStorer.wt",split_deepen_min_child=0,split_deepen_per_child=0,split_pct=90,type=file,value_format=u
Data
table:_mdb_catalog
+\00\00\00\12numRecords\00\11\00\00\00\00\00\00\00\12dataSize\00f\19\00\00\00\00\00\00\00
table:admin/collection/0-7029101439676270912
+\00\00\00\12numRecords\00\01\00\00\00\00\00\00\00\12dataSize\00;\00\00\00\00\00\00\00\00
table:config/collection/25-7029101439676270912
+\00\00\00\12numRecords\00\00\00\00\00\00\00\00\00\12dataSize\00\00\00\00\00\00\00\00\00\00
table:local/collection/2-7029101439676270912
+\00\00\00\12numRecords\00\0b\00\00\00\00\00\00\00\12dataSize\00\0dN\00\00\00\00\00\00\00
table:sbtest/collection/11-7029101439676270912
+\00\00\00\12numRecords\00E\d3\00\00\00\00\00\00\12dataSize\00Oa?\01\00\00\00\00\00
table:sbtest/collection/13-7029101439676270912
+\00\00\00\12numRecords\00\df;\00\00\00\00\00\00\12dataSize\00\1d\82Z\00\00\00\00\00\00
table:sbtest/collection/15-7029101439676270912
+\00\00\00\12numRecords\00\82/\00\00\00\00\00\00\12dataSize\00\86\d1G\00\00\00\00\00\00
table:sbtest/collection/17-7029101439676270912
+\00\00\00\12numRecords\00\0dF\00\00\00\00\00\00\12dataSize\00\a7\e5i\00\00\00\00\00\00
table:sbtest/collection/19-7029101439676270912
+\00\00\00\12numRecords\00\18R\00\00\00\00\00\00\12dataSize\00H\1a|\00\00\00\00\00\00
table:sbtest/collection/21-7029101439676270912
+\00\00\00\12numRecords\00\a0\87\00\00\00\00\00\00\12dataSize\00\e0\06\cd\00\00\00\00\00\00
table:sbtest/collection/23-7029101439676270912
+\00\00\00\12numRecords\00d+\00\00\00\00\00\00\12dataSize\00,\98A\00\00\00\00\00\00
table:sbtest/collection/4-7029101439676270912
+\00\00\00\12numRecords\00\dbd\00\00\00\00\00\00\12dataSize\00\11w\98\00\00\00\00\00\00
table:sbtest/collection/6-7029101439676270912
+\00\00\00\12numRecords\00\9ez\07\00\00\00\00\00\12dataSize\00\8aXN\0b\00\00\00\00\00
table:sbtest/collection/8-7029101439676270912
+\00\00\00\12numRecords\00\c8\02\00\00\00\00\00\00\12dataSize\00\ec\fb\0f\00\00\00\00\00\00
table:sbtest/collection/9-7029101439676270912
+\00\00\00\12numRecords\00\d85\00\00\00\00\00\00\12dataSize\00\88eQ\00\00\00\00\00\00
table:xxxx/collection/0-2872068773297699689
+\00\00\00\12numRecords\00\0a\00\00\00\00\00\00\00\12dataSize\00\99\02\00\00\00\00\00\00\00
table:xxxx/collection/2-2872068773297699689
+\00\00\00\12numRecords\00\08\00\00\00\00\00\00\00\12dataSize\00\eb\0d\00\00\00\00\00\00\00

[root@bogon mongo]# grep "WiredTigerSizeStorer::storeInto " /data/logs/mongod.log 
2018-11-06T17:27:25.737+0800 D STORAGE  [WTCheckpointThread] WiredTigerSizeStorer::storeInto table:_mdb_catalog -> { numRecords: 15, dataSize: 5898 }
2018-11-06T17:27:25.738+0800 D STORAGE  [WTCheckpointThread] WiredTigerSizeStorer::storeInto table:admin/collection/0-7029101439676270912 -> { numRecords: 1, dataSize: 59 }
2018-11-06T17:27:25.738+0800 D STORAGE  [WTCheckpointThread] WiredTigerSizeStorer::storeInto table:config/collection/25-7029101439676270912 -> { numRecords: 0, dataSize: 0 }
2018-11-06T17:27:25.738+0800 D STORAGE  [WTCheckpointThread] WiredTigerSizeStorer::storeInto table:local/collection/2-7029101439676270912 -> { numRecords: 7, dataSize: 12743 }
*/
//WiredTigerKVEngine::WiredTigerKVEngine中初始化，对应WiredTigerKVEngine._sizeStorerUri="table:sizeStorer"
//WiredTigerKVEngine::WiredTigerKVEngine中构造使用   sizeStorer.wt文件的操作
WiredTigerSizeStorer::WiredTigerSizeStorer(WT_CONNECTION* conn,
                                           const std::string& storageUri,
                                           bool logSizeStorerTable,
                                           bool readOnly)
    : _session(conn) {
    WT_SESSION* session = _session.getSession();

    std::string config = WiredTigerCustomizationHooks::get(getGlobalServiceContext())
                             ->getTableCreateConfig(storageUri);
    if (!readOnly) {
        invariantWTOK(session->create(session, storageUri.c_str(), config.c_str()));
        const bool keepOldLoggingSettings = true;
        if (keepOldLoggingSettings) {
            logSizeStorerTable = true;
        }
        uassertStatusOK(
            WiredTigerUtil::setTableLogging(session, storageUri.c_str(), logSizeStorerTable));
    }

    invariantWTOK(
        session->open_cursor(session, storageUri.c_str(), NULL, "overwrite=true", &_cursor));

    _magic = MAGIC;
}

WiredTigerSizeStorer::~WiredTigerSizeStorer() {
    // This shouldn't be necessary, but protects us if we screw up.
    stdx::lock_guard<stdx::mutex> cursorLock(_cursorMutex);

    _magic = 11111;
    _cursor->close(_cursor);
}

void WiredTigerSizeStorer::_checkMagic() const {
    if (MONGO_likely(_magic == MAGIC))
        return;
    log() << "WiredTigerSizeStorer magic wrong: " << _magic;
    invariant(_magic == MAGIC);
}

void WiredTigerSizeStorer::onCreate(WiredTigerRecordStore* rs,
                                    long long numRecords,
                                    long long dataSize) {
    _checkMagic();
    stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
    Entry& entry = _entries[rs->getURI()];
    entry.rs = rs;
    entry.numRecords = numRecords;
    entry.dataSize = dataSize;
    entry.dirty = true;
}

void WiredTigerSizeStorer::onDestroy(WiredTigerRecordStore* rs) {
    _checkMagic();
    stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
    Entry& entry = _entries[rs->getURI()];
    entry.numRecords = rs->numRecords(NULL);
    entry.dataSize = rs->dataSize(NULL);
    entry.dirty = true;
    entry.rs = NULL;
}

//WiredTigerRecordStore::_increaseDataSize
//修改_entries[uri]的值，也就是修改内存中的值中调用  
//WiredTigerSizeStorer::storeToCache和WiredTigerSizeStorer::loadFromCache对应
void WiredTigerSizeStorer::storeToCache(StringData uri, long long numRecords, long long dataSize) {
    _checkMagic();
    stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
    Entry& entry = _entries[uri.toString()];
    entry.numRecords = numRecords;
    entry.dataSize = dataSize;
    entry.dirty = true;
}

//WiredTigerSizeStorer::storeToCache和WiredTigerSizeStorer::loadFromCache对应
//获取_entries[uri]的内容返回  WiredTigerRecordStore::postConstructorInit中调用
//db.coll.count()操作也只是读内存数据。实际上就是调用该接口
void WiredTigerSizeStorer::loadFromCache(StringData uri,
                                         long long* numRecords,
                                         long long* dataSize) const {
    _checkMagic();
    stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
    Map::const_iterator it = _entries.find(uri.toString());
    if (it == _entries.end()) {
        *numRecords = 0;
        *dataSize = 0;
        return;
    }
    *numRecords = it->second.numRecords;
    *dataSize = it->second.dataSize;
}

//从sizeStorer.wt读取数据存入cache相关结构中
void WiredTigerSizeStorer::fillCache() {
    stdx::lock_guard<stdx::mutex> cursorLock(_cursorMutex);
    _checkMagic();

	//typedef std::map<std::string, Entry> Map; 
    Map m;
    {
        // Seek to beginning if needed.
        invariantWTOK(_cursor->reset(_cursor));

        // Intentionally ignoring return value.
        ON_BLOCK_EXIT(_cursor->reset, _cursor);

        int cursorNextRet;
        while ((cursorNextRet = _cursor->next(_cursor)) != WT_NOTFOUND) {
            invariantWTOK(cursorNextRet);

            WT_ITEM key;
            WT_ITEM value;
            invariantWTOK(_cursor->get_key(_cursor, &key));
            invariantWTOK(_cursor->get_value(_cursor, &value));
            std::string uriKey(reinterpret_cast<const char*>(key.data), key.size);
            BSONObj data(reinterpret_cast<const char*>(value.data));

            LOG(2) << "WiredTigerSizeStorer::loadFrom " << uriKey << " -> " << redact(data);

            Entry& e = m[uriKey];
            e.numRecords = data["numRecords"].safeNumberLong();
            e.dataSize = data["dataSize"].safeNumberLong();
            e.dirty = false;
            e.rs = NULL;
        }
    }

    stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
    _entries.swap(m);
}

//同步到wiredtiger层  参考http://www.mongoing.com/archives/5476
//mongodb使用WiredTigerSizeStorer做表的辅助信息的内存缓存，这些内存数据罗盘地通过_sizeStorerSyncTracker(cs, 100000, Seconds(60))定时器触发
//WiredTigerKVEngine::syncSizeInfo中调用
void WiredTigerSizeStorer::syncCache(bool syncToDisk) {
    stdx::lock_guard<stdx::mutex> cursorLock(_cursorMutex);
    _checkMagic();

    Map myMap;
    {
        stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
        for (Map::iterator it = _entries.begin(); it != _entries.end(); ++it) {
            std::string uriKey = it->first;
            Entry& entry = it->second;
            if (entry.rs) {
                if (entry.dataSize != entry.rs->dataSize(NULL)) {
                    entry.dataSize = entry.rs->dataSize(NULL);
                    entry.dirty = true;
                }
                if (entry.numRecords != entry.rs->numRecords(NULL)) {
                    entry.numRecords = entry.rs->numRecords(NULL);
                    entry.dirty = true;
                }
            }

            if (!entry.dirty)
                continue;
            myMap[uriKey] = entry;
        }
    }

    if (myMap.empty())
        return;  // Nothing to do.

	//把numRecords和datasize更新到wiredtiger
    WT_SESSION* session = _session.getSession();
    invariantWTOK(session->begin_transaction(session, syncToDisk ? "sync=true" : ""));
    ScopeGuard rollbacker = MakeGuard(session->rollback_transaction, session, "");

    for (Map::iterator it = myMap.begin(); it != myMap.end(); ++it) {
        string uriKey = it->first;
        Entry& entry = it->second;

        BSONObj data;
        {
            BSONObjBuilder b;
            b.append("numRecords", entry.numRecords);
            b.append("dataSize", entry.dataSize);
            data = b.obj();
        }

        LOG(2) << "WiredTigerSizeStorer::storeInto " << uriKey << " -> " << redact(data);

        WiredTigerItem key(uriKey.c_str(), uriKey.size());
        WiredTigerItem value(data.objdata(), data.objsize());
        _cursor->set_key(_cursor, key.Get());
        _cursor->set_value(_cursor, value.Get());
        invariantWTOK(_cursor->insert(_cursor));
    }

    invariantWTOK(_cursor->reset(_cursor));

    rollbacker.Dismiss();
    invariantWTOK(session->commit_transaction(session, NULL));

    {
        stdx::lock_guard<stdx::mutex> lk(_entriesMutex);
        for (Map::iterator it = _entries.begin(); it != _entries.end(); ++it) {
            it->second.dirty = false;
        }
    }
}
}
