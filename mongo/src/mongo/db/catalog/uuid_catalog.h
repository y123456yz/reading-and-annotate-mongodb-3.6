/**
 *    Copyright (C) 2017 MongoDB Inc.
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

#include <unordered_map>

#include "mongo/base/disallow_copying.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/service_context.h"
#include "mongo/stdx/functional.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * This class comprises a UUID to collection catalog, allowing for efficient
 * collection lookup by UUID.
 这个类包含一个UUID到集合目录，允许通过UUID进行高效的集合查找
 */
using CollectionUUID = UUID;
class Database;

//一个表对应一个UUID，通过该类管理，这个类包含一个UUID到集合目录，允许通过UUID进行高效的集合查找
class getCatalog {
    MONGO_DISALLOW_COPYING(UUIDCatalog);

public:
    static UUIDCatalog& get(ServiceContext* svcCtx);
    static UUIDCatalog& get(OperationContext* opCtx);
    UUIDCatalog() = default;

    /**
     * This function inserts the entry for uuid, coll into the UUID Collection. It is called by
     * the op observer when a collection is created.
     */
    void onCreateCollection(OperationContext* opCtx, Collection* coll, CollectionUUID uuid);

    /**
     * This function removes the entry for uuid from the UUID catalog. It is called by the op
     * observer when a collection is dropped.
     */
    void onDropCollection(OperationContext* opCtx, CollectionUUID uuid);

    /**
     * Combination of onDropCollection and onCreateCollection.
     * 'getNewCollection' is a function that returns collection to be registered when the current
     * write unit of work is committed.
     */
    using GetNewCollectionFunction = stdx::function<Collection*()>;
    void onRenameCollection(OperationContext* opCtx,
                            GetNewCollectionFunction getNewCollection,
                            CollectionUUID uuid);

    /**
     * Implies onDropCollection for all collections in db, but is not transactional.
     */
    void onCloseDatabase(Database* db);

    void registerUUIDCatalogEntry(CollectionUUID uuid, Collection* coll);
    Collection* removeUUIDCatalogEntry(CollectionUUID uuid);

    /* This function gets the Collection* pointer that corresponds to
     * CollectionUUID uuid. The required locks should be obtained prior
     * to calling this function, or else the found Collection pointer
     * might no longer be valid when the call returns.
     */
    Collection* lookupCollectionByUUID(CollectionUUID uuid) const;

    /* This function gets the NamespaceString from the Collection* pointer that
     * corresponds to CollectionUUID uuid. If there is no such pointer, an empty
     * NamespaceString is returned.
     */
    NamespaceString lookupNSSByUUID(CollectionUUID uuid) const;

    /**
     * Return the UUID lexicographically preceding `uuid` in the database named by `db`.
     *
     * Return `boost::none` if `uuid` is not found, or is the first UUID in that database.
     */
    boost::optional<CollectionUUID> prev(const StringData& db, CollectionUUID uuid);

    /**
     * Return the UUID lexicographically following `uuid` in the database named by `db`.
     *
     * Return `boost::none` if `uuid` is not found, or is the last UUID in that database.
     */
    boost::optional<CollectionUUID> next(const StringData& db, CollectionUUID uuid);

private:
    const std::vector<CollectionUUID>& _getOrdering_inlock(const StringData& db,
                                                           const stdx::lock_guard<stdx::mutex>&);

    mutable mongo::stdx::mutex _catalogLock;

    /**
     * Map from database names to ordered `vector`s of their UUIDs.
     *
     * Works as a cache of such orderings: every ordering in this map is guaranteed to be valid, but
     * not all databases are guaranteed to have an ordering in it.
     */
    //赋值参考UUIDCatalog::_getOrdering_inlock 
    //根据_catalog map表中的uuid进行排序，排序好后存入_orderedCollections
    //同一个DB下面的CollectionUUID，放到一起，放到二位数组的第一层，例如_orderedCollections[db][]
    StringMap<std::vector<CollectionUUID>> _orderedCollections;
    //UUIDCatalog::registerUUIDCatalogEntry添加uuid及collection到_catalog，lookupCollectionByUUID中查找

   
    //AutoGetDb::AutoGetDb或者AutoGetOrCreateDb::AutoGetOrCreateDb->DatabaseHolderImpl::get从DatabaseHolderImpl._dbs数组查找获取Database
    //DatabaseImpl::createCollection创建collection的表全部添加到DatabaseImpl._collections数组中
    //AutoGetCollection::AutoGetCollection通过Database::getCollection或者UUIDCatalog::lookupCollectionByUUID(从UUIDCatalog._catalog数组通过查找uuid可以获取collection表信息)
    //注意AutoGetCollection::AutoGetCollection构造函数可以是uuid，也有一个构造函数是nss，也就是可以通过uuid查找，也可以通过nss查找
   //包含所有DB的uuid信息，最好排好序存入_orderedCollections[i][j]二维数组，同一个DB的i相同，通过j区分同一个db的不同的uuid
   mongo::stdx::unordered_map<CollectionUUID, Collection*, CollectionUUID::Hash> _catalog;
};

}  // namespace mongo
