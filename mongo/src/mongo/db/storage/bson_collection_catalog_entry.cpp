// bson_collection_catalog_entry.cpp

/**
 *    Copyright (C) 2014 MongoDB Inc.
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

#include "mongo/db/storage/bson_collection_catalog_entry.h"

#include <algorithm>
#include <numeric>

#include "mongo/db/field_ref.h"

namespace mongo {

namespace {

// An index will fail to get created if the size in bytes of its key pattern is greater than 2048.
// We use that value to represent the largest number of path components we could ever possibly
// expect to see in an indexed field.
const size_t kMaxKeyPatternPathLength = 2048;
char multikeyPathsEncodedAsBytes[kMaxKeyPatternPathLength];

/**
 * Encodes 'multikeyPaths' as binary data and appends it to 'bob'.
 *
 * For example, consider the index {'a.b': 1, 'a.c': 1} where the paths "a" and "a.b" cause it to be
 * multikey. The object {'a.b': HexData('0101'), 'a.c': HexData('0100')} would then be appended to
 * 'bob'.
 */
void appendMultikeyPathsAsBytes(BSONObj keyPattern,
                                const MultikeyPaths& multikeyPaths,
                                BSONObjBuilder* bob) {
    size_t i = 0;
    for (const auto keyElem : keyPattern) {
        StringData keyName = keyElem.fieldNameStringData();
        size_t numParts = FieldRef{keyName}.numParts();
        invariant(numParts > 0);
        invariant(numParts <= kMaxKeyPatternPathLength);

        std::fill_n(multikeyPathsEncodedAsBytes, numParts, 0);
        for (const auto multikeyComponent : multikeyPaths[i]) {
            multikeyPathsEncodedAsBytes[multikeyComponent] = 1;
        }
        bob->appendBinData(keyName, numParts, BinDataGeneral, &multikeyPathsEncodedAsBytes[0]);

        ++i;
    }
}

/**
 * Parses the path-level multikey information encoded as binary data from 'multikeyPathsObj' and
 * sets 'multikeyPaths' as that value.
 *
 * For example, consider the index {'a.b': 1, 'a.c': 1} where the paths "a" and "a.b" cause it to be
 * multikey. The binary data {'a.b': HexData('0101'), 'a.c': HexData('0100')} would then be parsed
 * into std::vector<std::set<size_t>>{{0U, 1U}, {0U}}.
 */
void parseMultikeyPathsFromBytes(BSONObj multikeyPathsObj, MultikeyPaths* multikeyPaths) {
    invariant(multikeyPaths);
    for (auto elem : multikeyPathsObj) {
        std::set<size_t> multikeyComponents;
        int len;
        const char* data = elem.binData(len);
        invariant(len > 0);
        invariant(static_cast<size_t>(len) <= kMaxKeyPatternPathLength);

        for (int i = 0; i < len; ++i) {
            if (data[i]) {
                multikeyComponents.insert(i);
            }
        }
        multikeyPaths->push_back(multikeyComponents);
    }
}

}  // namespace

BSONCollectionCatalogEntry::BSONCollectionCatalogEntry(StringData ns)
    : CollectionCatalogEntry(ns) {}

CollectionOptions BSONCollectionCatalogEntry::getCollectionOptions(OperationContext* opCtx) const {
	//KVCollectionCatalogEntry::_getMetaData获取元数据信息
	MetaData md = _getMetaData(opCtx);
    return md.options;
}

int BSONCollectionCatalogEntry::getTotalIndexCount(OperationContext* opCtx) const {
    MetaData md = _getMetaData(opCtx);

    return static_cast<int>(md.indexes.size());
}

int BSONCollectionCatalogEntry::getCompletedIndexCount(OperationContext* opCtx) const {
    MetaData md = _getMetaData(opCtx);

    int num = 0;
    for (unsigned i = 0; i < md.indexes.size(); i++) {
        if (md.indexes[i].ready)
            num++;
    }
    return num;
}

BSONObj BSONCollectionCatalogEntry::getIndexSpec(OperationContext* opCtx,
                                                 StringData indexName) const {
    MetaData md = _getMetaData(opCtx);

    int offset = md.findIndexOffset(indexName);
    invariant(offset >= 0);
    return md.indexes[offset].spec.getOwned();
}

//IndexCatalogImpl::init调用，获取所有的所有名
//从元数据文件"_mdb_catalog.wt"中获取索引信息
void BSONCollectionCatalogEntry::getAllIndexes(OperationContext* opCtx,
                                               std::vector<std::string>* names) const {
    MetaData md = _getMetaData(opCtx);

    for (unsigned i = 0; i < md.indexes.size(); i++) {
        names->push_back(md.indexes[i].spec["name"].String());
    }
}

//
bool BSONCollectionCatalogEntry::isIndexMultikey(OperationContext* opCtx,
                                                 StringData indexName,
                                                 MultikeyPaths* multikeyPaths) const {
    MetaData md = _getMetaData(opCtx);

    int offset = md.findIndexOffset(indexName);
    invariant(offset >= 0);

    if (multikeyPaths && !md.indexes[offset].multikeyPaths.empty()) {
        *multikeyPaths = md.indexes[offset].multikeyPaths;
    }

    return md.indexes[offset].multikey;
}

RecordId BSONCollectionCatalogEntry::getIndexHead(OperationContext* opCtx,
                                                  StringData indexName) const {
    MetaData md = _getMetaData(opCtx);

    int offset = md.findIndexOffset(indexName);
    invariant(offset >= 0);
    return md.indexes[offset].head;
}

/* 写数据流程
Breakpoint 1, mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d14b400, opCtx=0x7fe69d14db80) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
309         return _catalog->getMetaData(opCtx, ns().toString());
(gdb) bt
#0  mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d14b400, opCtx=0x7fe69d14db80) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
#1  0x00007fe69567c7e2 in mongo::BSONCollectionCatalogEntry::isIndexReady (this=<optimized out>, opCtx=<optimized out>, indexName=...) at src/mongo/db/storage/bson_collection_catalog_entry.cpp:172
#2  0x00007fe69585e4a6 in _catalogIsReady (opCtx=<optimized out>, this=0x7fe699a36500) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:332
#3  mongo::IndexCatalogEntryImpl::isReady (this=0x7fe699a36500, opCtx=<optimized out>) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:167
#4  0x00007fe695854ee9 in isReady (opCtx=0x7fe69d14db80, this=0x7fe699a5b3b8) at src/mongo/db/catalog/index_catalog_entry.h:224
#5  mongo::IndexCatalogImpl::IndexIteratorImpl::_advance (this=this@entry=0x7fe69d3d3680) at src/mongo/db/catalog/index_catalog_impl.cpp:1170
#6  0x00007fe695855047 in mongo::IndexCatalogImpl::IndexIteratorImpl::more (this=0x7fe69d3d3680) at src/mongo/db/catalog/index_catalog_impl.cpp:1129
#7  0x00007fe69585315d in more (this=<synthetic pointer>) at src/mongo/db/catalog/index_catalog.h:104
#8  mongo::IndexCatalogImpl::findIdIndex (this=<optimized out>, opCtx=<optimized out>) at src/mongo/db/catalog/index_catalog_impl.cpp:1182
#9  0x00007fe69583af61 in findIdIndex (opCtx=0x7fe69d14db80, this=0x7fe69984c038) at src/mongo/db/catalog/index_catalog.h:331
#10 mongo::CollectionImpl::insertDocuments (this=0x7fe69984bfc0, opCtx=0x7fe69d14db80, begin=..., end=..., opDebug=0x7fe69d145938, enforceQuota=true, fromMigrate=false) at src/mongo/db/catalog/collection_impl.cpp:350
#11 0x00007fe6957ce352 in insertDocuments (fromMigrate=false, enforceQuota=true, opDebug=<optimized out>, end=..., begin=..., opCtx=0x7fe69d14db80, this=<optimized out>) at src/mongo/db/catalog/collection.h:498
#12 mongo::(anonymous namespace)::insertDocuments (opCtx=0x7fe69d14db80, collection=<optimized out>, begin=begin@entry=..., end=end@entry=...) at src/mongo/db/ops/write_ops_exec.cpp:329
#13 0x00007fe6957d4026 in operator() (__closure=<optimized out>) at src/mongo/db/ops/write_ops_exec.cpp:406
#14 writeConflictRetry<mongo::(anonymous namespace)::insertBatchAndHandleErrors(mongo::OperationContext*, const mongo::write_ops::Insert&, 
std::vector<mongo::InsertStatement>&, mongo::(anonymous namespace)::LastOpFixer*, mongo::WriteResult*)::<lambda()> > (f=<optimized out>, ns=..., opStr=..., opCtx=0x7fe69d14db80) at src/mongo/db/concurrency/write_conflict_exception.h:91
*/
// These are the index specs of indexes that were "leftover".
// "Leftover" means they were unfinished when a mongod shut down.
// Certain operations are prohibited until someone fixes.
// Retrieve by calling getAndClearUnfinishedIndexes().
//索引没有执行完成，加到_unfinishedIndexes

//IndexCatalogImpl::init调用，索引是否添加完成
bool BSONCollectionCatalogEntry::isIndexReady(OperationContext* opCtx, StringData indexName) const {
    MetaData md = _getMetaData(opCtx);

    int offset = md.findIndexOffset(indexName);
    invariant(offset >= 0);
    return md.indexes[offset].ready;
}

/*
读数据流程走这里
(gdb) bt
#0  mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d7d6580, opCtx=0x7fe69d3a7280) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
#1  0x00007fe69567ca22 in mongo::BSONCollectionCatalogEntry::getIndexPrefix (this=<optimized out>, opCtx=<optimized out>, indexName=...) at src/mongo/db/storage/bson_collection_catalog_entry.cpp:181
#2  0x00007fe69585eac8 in mongo::IndexCatalogEntryImpl::IndexCatalogEntryImpl (this=0x7fe69d8061c0, this_=0x7fe699a5b5f0, opCtx=0x7fe69d3a7280, ns=..., collection=0x7fe69d7d6580, descriptor=..., infoCache=0x7fe69d7df3f0)
    at src/mongo/db/catalog/index_catalog_entry_impl.cpp:103
#3  0x00007fe69585f396 in make_unique<mongo::IndexCatalogEntryImpl, mongo::IndexCatalogEntry* const&, mongo::OperationContext* const&, mongo::StringData const&, mongo::CollectionCatalogEntry* const&, std::unique_ptr<mongo::IndexDescriptor, std::default_delete<mongo::IndexDescriptor> >, mongo::CollectionInfoCache* const&> () at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:765
#4  operator() (__closure=<optimized out>, infoCache=0x7fe69d7df3f0, descriptor=..., collection=0x7fe69d7d6580, ns=..., opCtx=<optimized out>, this_=<optimized out>) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:65
#5  std::_Function_handler<std::unique_ptr<mongo::IndexCatalogEntry::Impl, std::default_delete<mongo::IndexCatalogEntry::Impl> >(mongo::IndexCatalogEntry*, mongo::OperationContext*, mongo::StringData, mongo::CollectionCatalogEntry*, std::unique_ptr<mongo::IndexDescriptor, std::default_delete<mongo::IndexDescriptor> >, mongo::CollectionInfoCache*), mongo::(anonymous namespace)::_mongoInitializerFunction_InitializeIndexCatalogEntryFactory(mongo::InitializerContext*)::<lambda(mongo::IndexCatalogEntry*, mongo::OperationContext*, mongo::StringData, mongo::CollectionCatalogEntry*, std::unique_ptr<mongo::IndexDescriptor, std::default_delete<mongo::IndexDescriptor> >, mongo::CollectionInfoCache*)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b666e>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b6673>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b6678>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b667d>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b6682>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b6687>) (__functor=..., __args#0=<optimized out>, 
    __args#1=<optimized out>, __args#2=<optimized out>, __args#3=<optimized out>, __args#4=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b6682>, 
    __args#5=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x83c9767, DIE 0x84b6687>) at /usr/local/include/c++/5.4.0/functional:1857
#6  0x00007fe695af6ab3 in operator() (__args#5=0x7fe69d7df3f0, __args#4=..., __args#3=0x7fe69d7d6580, __args#2=..., __args#1=0x7fe69d3a7280, __args#0=0x7fe699a5b5f0, this=0x7fe697a31820 <mongo::(anonymous namespace)::factory>)
    at /usr/local/include/c++/5.4.0/functional:2267
#7  mongo::IndexCatalogEntry::makeImpl (this_=<optimized out>, opCtx=<optimized out>, ns=..., collection=<optimized out>, descriptor=..., infoCache=0x7fe69d7df3f0) at src/mongo/db/catalog/index_catalog_entry.cpp:55
#8  0x00007fe695af6c05 in mongo::IndexCatalogEntry::IndexCatalogEntry (this=<optimized out>, opCtx=<optimized out>, ns=..., collection=<optimized out>, descriptor=..., infoCache=0x7fe69d7df3f0)
    at src/mongo/db/catalog/index_catalog_entry.cpp:65
#9  0x00007fe69585cf52 in make_unique<mongo::IndexCatalogEntry, mongo::OperationContext*&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, mongo::CollectionCatalogEntry*, std::unique_ptr<mongo::IndexDescriptor, std::default_delete<mongo::IndexDescriptor> >, mongo::CollectionInfoCache*> () at /usr/local/include/c++/5.4.0/bits/unique_ptr.h:765
#10 mongo::IndexCatalogImpl::_setupInMemoryStructures (this=0x7fe6998451e0, opCtx=0x7fe69d3a7280, descriptor=..., initFromDisk=<optimized out>) at src/mongo/db/catalog/index_catalog_impl.cpp:182
#11 0x00007fe696b7fcbe in mongo::IndexCatalog::_setupInMemoryStructures (this=<optimized out>, opCtx=<optimized out>, descriptor=..., initFromDisk=initFromDisk@entry=false) at src/mongo/db/catalog/index_catalog.cpp:60
#12 0x00007fe695854c98 in _setupInMemoryStructures (initFromDisk=false, descriptor=..., opCtx=<optimized out>, this_=<optimized out>) at src/mongo/db/catalog/index_catalog_impl.h:460
#13 mongo::IndexCatalogImpl::IndexBuildBlock::init (this=0x7fe69cea7cc0) at src/mongo/db/catalog/index_catalog_impl.cpp:425
#14 0x00007fe69586716e in mongo::MultiIndexBlockImpl::init (this=0x7fe69d3d3080, indexSpecs=...) at src/mongo/db/catalog/index_create_impl.cpp:259
#15 0x00007fe695725b31 in init (specs=..., this=0x7fe689c84490) at src/mongo/db/catalog/index_create.h:180
#16 operator() (__closure=0x7fe689c845c0) at src/mongo/db/commands/create_indexes.cpp:331
#17 mongo::writeConflictRetry<mongo::CmdCreateIndex::errmsgRun(mongo::OperationContext*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, mongo::BSONObj const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >&, mongo::BSONObjBuilder&)::{lambda()#3}>(mongo::OperationContext*, mongo::StringData, mongo::CmdCreateIndex::errmsgRun(mongo::OperationContext*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, mongo::BSONObj const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >&, mongo::BSONObjBuilder&)::{lambda()#3}, mongo::CmdCreateIndex::errmsgRun(mongo::OperationContext*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, mongo::BSONObj const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >&, mongo::BSONObjBuilder&)::{lambda()#3}&&) (opCtx=0x7fe69d3a7280, opStr=..., ns=..., f=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongod, CU 0x3dac431, DIE 0x3ec798b>) at src/mongo/db/concurrency/write_conflict_exception.h:91
#18 0x00007fe695729809 in mongo::CmdCreateIndex::errmsgRun (this=<optimized out>, opCtx=0x7fe69d3a7280, dbname=..., cmdObj=..., errmsg=..., result=...) at src/mongo/db/commands/create_indexes.cpp:332
#19 0x00007fe69677e366 in mongo::ErrmsgCommandDeprecated::run (this=this@entry=0x7fe697a282e0 <mongo::cmdCreateIndex>, opCtx=opCtx@entry=0x7fe69d3a7280, db=..., cmdObj=..., result=...) at src/mongo/db/commands.cpp:424
#20 0x00007fe69677fb36 in mongo::BasicCommand::enhancedRun (this=0x7fe697a282e0 <mongo::cmdCreateIndex>, opCtx=0x7fe69d3a7280, request=..., result=...) at src/mongo/db/commands.cpp:416
#21 0x00007fe69677c2df in mongo::Command::publicRun (this=0x7fe697a282e0 <mongo::cmdCreateIndex>, opCtx=0x7fe69d3a7280, request=..., result=...) at src/mongo/db/commands.cpp:354
#22 0x00007fe6956f8804 in runCommandImpl (startOperationTime=..., replyBuilder=0x7fe699843780, request=..., command=0x7fe697a282e0 <mongo::cmdCreateIndex>, opCtx=0x7fe69d3a7280) at src/mongo/db/service_entry_point_mongod.cpp:504
#23 mongo::(anonymous namespace)::execCommandDatabase (opCtx=0x7fe69d3a7280, command=command@entry=0x7fe697a282e0 <mongo::cmdCreateIndex>, request=..., replyBuilder=<optimized out>) at src/mongo/db/service_entry_point_mongod.cpp:757
#24 0x00007fe6956f936f in mongo::(anonymous namespace)::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fe689c84a00) at src/mongo/db/service_entry_point_mongod.cpp:878
#25 0x00007fe6956f936f in mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=<optimized out>, m=...)
#26 0x00007fe6956fa1d1 in runCommands (message=..., opCtx=0x7fe69d3a7280) at src/mongo/db/service_entry_point_mongod.cpp:888
#27 mongo::ServiceEntryPointMongod::handleRequest (this=<optimized out>, opCtx=0x7fe69d3a7280, m=...) at src/mongo/db/service_entry_point_mongod.cpp:1161

*/
KVPrefix BSONCollectionCatalogEntry::getIndexPrefix(OperationContext* opCtx,
                                                    StringData indexName) const {
    MetaData md = _getMetaData(opCtx);
    int offset = md.findIndexOffset(indexName);
    invariant(offset >= 0);
    return md.indexes[offset].prefix;
}

// --------------------------

void BSONCollectionCatalogEntry::IndexMetaData::updateTTLSetting(long long newExpireSeconds) {
    BSONObjBuilder b;
    for (BSONObjIterator bi(spec); bi.more();) {
        BSONElement e = bi.next();
        if (e.fieldNameStringData() == "expireAfterSeconds") {
            continue;
        }
        b.append(e);
    }

    b.append("expireAfterSeconds", newExpireSeconds);
    spec = b.obj();
}

// --------------------------

int BSONCollectionCatalogEntry::MetaData::findIndexOffset(StringData name) const {
    for (unsigned i = 0; i < indexes.size(); i++)
        if (indexes[i].name() == name)
            return i;
    return -1;
}

bool BSONCollectionCatalogEntry::MetaData::eraseIndex(StringData name) {
    int indexOffset = findIndexOffset(name);

    if (indexOffset < 0) {
        return false;
    }

    indexes.erase(indexes.begin() + indexOffset);
    return true;
}

void BSONCollectionCatalogEntry::MetaData::rename(StringData toNS) {
    ns = toNS.toString();
    for (size_t i = 0; i < indexes.size(); i++) {
        BSONObj spec = indexes[i].spec;
        BSONObjBuilder b;
        // Add the fields in the same order they were in the original specification.
        for (auto&& elem : spec) {
            if (elem.fieldNameStringData() == "ns") {
                b.append("ns", toNS);
            } else {
                b.append(elem);
            }
        }
        indexes[i].spec = b.obj();
    }
}

KVPrefix BSONCollectionCatalogEntry::MetaData::getMaxPrefix() const {
    // Use the collection prefix as the initial max value seen. Then compare it with each index
    // prefix. Note the oplog has no indexes so the vector of 'IndexMetaData' may be empty.
    return std::accumulate(
        indexes.begin(), indexes.end(), prefix, [](KVPrefix max, IndexMetaData index) {
            return max < index.prefix ? index.prefix : max;
        });
}

BSONObj BSONCollectionCatalogEntry::MetaData::toBSON() const {
    BSONObjBuilder b;
    b.append("ns", ns);
    b.append("options", options.toBSON());
    {
        BSONArrayBuilder arr(b.subarrayStart("indexes"));
        for (unsigned i = 0; i < indexes.size(); i++) {
            BSONObjBuilder sub(arr.subobjStart());
            sub.append("spec", indexes[i].spec);
            sub.appendBool("ready", indexes[i].ready);
            sub.appendBool("multikey", indexes[i].multikey);

            if (!indexes[i].multikeyPaths.empty()) {
                BSONObjBuilder subMultikeyPaths(sub.subobjStart("multikeyPaths"));
                appendMultikeyPathsAsBytes(indexes[i].spec.getObjectField("key"),
                                           indexes[i].multikeyPaths,
                                           &subMultikeyPaths);
                subMultikeyPaths.doneFast();
            }

            sub.append("head", static_cast<long long>(indexes[i].head.repr()));
            sub.append("prefix", indexes[i].prefix.toBSONValue());
            sub.doneFast();
        }
        arr.doneFast();
    }
    b.append("prefix", prefix.toBSONValue());
    return b.obj();
}

void BSONCollectionCatalogEntry::MetaData::parse(const BSONObj& obj) {
    ns = obj["ns"].valuestrsafe();

    if (obj["options"].isABSONObj()) {
        options.parse(obj["options"].Obj(), CollectionOptions::parseForStorage)
            .transitional_ignore();
    }

    BSONElement indexList = obj["indexes"];

    if (indexList.isABSONObj()) {
        for (BSONElement elt : indexList.Obj()) {
            BSONObj idx = elt.Obj();
            IndexMetaData imd;
            imd.spec = idx["spec"].Obj().getOwned();
            imd.ready = idx["ready"].trueValue();
            if (idx.hasField("head")) {
                imd.head = RecordId(idx["head"].Long());
            } else {
                imd.head = RecordId(idx["head_a"].Int(), idx["head_b"].Int());
            }
            imd.multikey = idx["multikey"].trueValue();

            if (auto multikeyPathsElem = idx["multikeyPaths"]) {
                parseMultikeyPathsFromBytes(multikeyPathsElem.Obj(), &imd.multikeyPaths);
            }

            imd.prefix = KVPrefix::fromBSONElement(idx["prefix"]);
            indexes.push_back(imd);
        }
    }

    prefix = KVPrefix::fromBSONElement(obj["prefix"]);
}
}
