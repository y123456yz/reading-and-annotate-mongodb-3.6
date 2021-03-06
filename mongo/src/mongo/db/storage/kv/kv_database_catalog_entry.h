/**
 *    Copyright (C) 2016 MongoDB Inc.
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

#include "mongo/db/storage/kv/kv_database_catalog_entry_base.h"

namespace mongo {
//KVDatabaseCatalogEntry继承KVDatabaseCatalogEntryBase，后者又继承DatabaseCatalogEntry，这样
//就把kv和catalog联系了起来

//KVDatabaseCatalogEntry和KVCollectionCatalogEntry很类似，一个针对库下面所有表的元数据管理，一个针对表
//的元数据管理


//通过KVStorageEngine::getDatabaseCatalogEntry获取


////KVStorageEngine初始化构造的时候调用
class KVDatabaseCatalogEntry : public KVDatabaseCatalogEntryBase {
public:
    using KVDatabaseCatalogEntryBase::KVDatabaseCatalogEntryBase;

    IndexAccessMethod* getIndex(OperationContext* opCtx,
                                const CollectionCatalogEntry* collection,
                                IndexCatalogEntry* index) final;
};
}  // namespace mongo
