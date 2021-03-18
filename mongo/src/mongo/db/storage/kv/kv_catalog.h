// kv_catalog.h

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

#pragma once

#include <map>
#include <memory>
#include <string>

#include "mongo/base/string_data.h"
#include "mongo/db/catalog/collection_options.h"
#include "mongo/db/record_id.h"
#include "mongo/db/storage/bson_collection_catalog_entry.h"
#include "mongo/db/storage/kv/kv_prefix.h"
#include "mongo/stdx/mutex.h"

namespace mongo {

class OperationContext;
class RecordStore;

//_mdb_catalog.wt存储元数据信息，具体内容如下:

/* db.user.ensureIndex({"name":1, "aihao.aa":1, "aihao.bb":1}) db.user.ensureIndex({"name":1, aa:1, bb:1, cc:1})索引对应日志如下：
2021-03-17T18:10:51.944+0800 D STORAGE  [conn-1] recording new metadata: 
{
	md: {
		ns: "test.user",
		options: {
			uuid: UUID("9a09f018-3fb3-4030-b658-680e512c93dd")
		},
		indexes: [{
			spec: {
				v: 2,
				key: {
					_id: 1
				},
				name: "_id_",
				ns: "test.user"
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				_id: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}, {
			spec: {
				v: 2,
				key: {
					name: 1.0,
					aihao.aa: 1.0,
					aihao.bb: 1.0
				},
				name: "name_1_aihao.aa_1_aihao.bb_1",
				ns: "test.user"
			},
			ready: true,
			multikey: true,
			multikeyPaths: {
				name: BinData(0, 00),       //说明是一个独立的字段
				aihao.aa: BinData(0, 0100), //说明是一个a.b类型的索引
				aihao.bb: BinData(0, 0100)  //说明是一个a.b类型的索引
			},
			head: 0,
			prefix: -1
		}, {
			spec: {
				v: 2,
				key: {
					name: 1.0,
					aa: 1.0,
					bb: 1.0,
					cc: 1.0
				},
				name: "name_1_aa_1_bb_1_cc_1",
				ns: "test.user"
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				name: BinData(0, 00),
				aa: BinData(0, 00),
				bb: BinData(0, 00),
				cc: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}],
		prefix: -1
	},
	idxIdent: {
		_id_: "test/index/2--8777216180098127804",
		name_1_aihao.aa_1_aihao.bb_1: "test/index/3--8777216180098127804",
		name_1_aa_1_bb_1_cc_1: "test/index/4--8777216180098127804"
	},
	ns: "test.user",
	ident: "test/collection/1--8777216180098127804"
}
*/

//[initandlisten] recording new metadata: { md: { ns: "admin.system.version", options: 
//{ uuid: UUID("d24324d6-5465-4634-9f8a-3d6c6f6af801") }, indexes: [ { spec: { v: 2, key: { _id: 1 }, 
//name: "_id_", ns: "admin.system.version" }, ready: true, multikey: false, multikeyPaths: 
//{ _id: BinData(0, 00) }, head: 0, prefix: -1 } ], prefix: -1 }, idxIdent: { _id_: "admin/index/1--9034870482849730886" }, 
//ns: "admin.system.version", ident: "admin/collection/0--9034870482849730886" }


//KVStorageEngine::KVStorageEngine中构造初始化，赋值给KVStorageEngine._catalog
//KVCollectionCatalogEntry._catalog为该类型
//KVStorageEngine._catalog(KVCatalog类型)

//对应数据目录的"_mdb_catalog.wt"相关操作
class KVCatalog { //collection对应的文件名字相关
public:
    class FeatureTracker;

    /**
     * @param rs - does NOT take ownership. The RecordStore must be thread-safe, in particular
     * with concurrent calls to RecordStore::find, updateRecord, insertRecord, deleteRecord and
     * dataFor. The KVCatalog does not utilize Cursors and those methods may omit further
     * protection.
     */
    KVCatalog(RecordStore* rs, bool directoryPerDb, bool directoryForIndexes);
    ~KVCatalog();

    void init(OperationContext* opCtx);

    void getAllCollections(std::vector<std::string>* out) const;

    /**
     * @return error or ident for instance
     */
    Status newCollection(OperationContext* opCtx,
                         StringData ns,
                         const CollectionOptions& options,
                         KVPrefix prefix);

    std::string getCollectionIdent(StringData ns) const;

    std::string getIndexIdent(OperationContext* opCtx, StringData ns, StringData idName) const;

    const BSONCollectionCatalogEntry::MetaData getMetaData(OperationContext* opCtx, StringData ns);
    void putMetaData(OperationContext* opCtx,
                     StringData ns,
                     BSONCollectionCatalogEntry::MetaData& md);

    Status renameCollection(OperationContext* opCtx,
                            StringData fromNS,
                            StringData toNS,
                            bool stayTemp);

    Status dropCollection(OperationContext* opCtx, StringData ns);

    std::vector<std::string> getAllIdentsForDB(StringData db) const;
    std::vector<std::string> getAllIdents(OperationContext* opCtx) const;

    bool isUserDataIdent(StringData ident) const;

    FeatureTracker* getFeatureTracker() const {
        invariant(_featureTracker);
        return _featureTracker.get();
    }

private:
    class AddIdentChange;
    class RemoveIdentChange;

    BSONObj _findEntry(OperationContext* opCtx, StringData ns, RecordId* out = NULL) const;

    /**
     * Generates a new unique identifier for a new "thing".
     * @param ns - the containing ns
     * @param kind - what this "thing" is, likely collection or index
     */
    std::string _newUniqueIdent(StringData ns, const char* kind);

    // Helpers only used by constructor and init(). Don't call from elsewhere.
    static std::string _newRand();
    bool _hasEntryCollidingWithRand() const;

    //对应数据目录的"_mdb_catalog.wt"，存储表相关元数据信息，例如表名 索引信息等
    RecordStore* _rs;  // not owned   对应WiredTigerRecordStore
    const bool _directoryPerDb;
    const bool _directoryForIndexes;

    // These two are only used for ident generation inside _newUniqueIdent.
    std::string _rand;  // effectively const after init() returns
    AtomicUInt64 _next;

    struct Entry {
        Entry() {}
        Entry(std::string i, RecordId l) : ident(i), storedLoc(l) {}
        std::string ident; //collection对应的文件名字（ident）   _newUniqueIdent生成
        RecordId storedLoc;
    };
    typedef std::map<std::string, Entry> NSToIdentMap;
    //所有集合信息存到这里面
    //更新_idents，记录下集合对应元数据信息，也就是集合路径  集合uuid 集合索引，以及在元数据_mdb_catalog.wt中的位置
    NSToIdentMap _idents;
    mutable stdx::mutex _identsLock;

    // Manages the feature document that may be present in the KVCatalog. '_featureTracker' is
    // guaranteed to be non-null after KVCatalog::init() is called.
    std::unique_ptr<FeatureTracker> _featureTracker;
};
}
