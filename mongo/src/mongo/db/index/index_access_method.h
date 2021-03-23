/**
*    Copyright (C) 2013-2014 MongoDB Inc.
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

#include <atomic>
#include <memory>

#include "mongo/base/disallow_copying.h"
#include "mongo/bson/simple_bsonobj_comparator.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/record_id.h"
#include "mongo/db/sorter/sorter.h"
#include "mongo/db/storage/sorted_data_interface.h"

namespace mongo {

extern AtomicBool failIndexKeyTooLong;

class BSONObjBuilder;
class MatchExpression;
class UpdateTicket;
struct InsertDeleteOptions;

/**
 * An IndexAccessMethod is the interface through which all the mutation, lookup, and
 * traversal of index entries is done. The class is designed so that the underlying index
 * data structure is opaque to the caller.
 *
 * IndexAccessMethods for existing indices are obtained through the system catalog.
 *
 * We assume the caller has whatever locks required.  This interface is not thread safe.
 *
 */
/*
IndexAccessMethod初始化实现见:  下面的这些文件中的类都继承自IndexAccessMethod
以下类都在IndexAccessMethod* KVDatabaseCatalogEntry::getIndex中实现，参考IndexAccessMethod* KVDatabaseCatalogEntry::getIndex

2d_access_method.cpp (src\mongo\db\index):    : IndexAccessMethod(btreeState, btree) {
Btree_access_method.cpp (src\mongo\db\index):    : IndexAccessMethod(btreeState, btree) {
Fts_access_method.cpp (src\mongo\db\index):    : IndexAccessMethod(btreeState, btree), _ftsSpec(btreeState->descriptor()->infoObj()) {}
Hash_access_method.cpp (src\mongo\db\index):    : IndexAccessMethod(btreeState, btree) {
Haystack_access_method.cpp (src\mongo\db\index):    : IndexAccessMethod(btreeState, btree) {
Index_access_method.cpp (src\mongo\db\index):IndexAccessMethod::IndexAccessMethod(IndexCatalogEntry* btreeState, SortedDataInterface* btree)
Index_access_method.h (src\mongo\db\index):    IndexAccessMethod(IndexCatalogEntry* btreeState, SortedDataInterface* btree);
Index_access_method.h (src\mongo\db\index):    virtual ~IndexAccessMethod() {}
Index_descriptor.h (src\mongo\db\index):    // "Internals" of accessing the index, used by IndexAccessMethod(s).
S2_access_method.cpp (src\mongo\db\index):    : IndexAccessMethod(btreeState, btree) {
*/ //操作接口，CRUD方法，注意是非线程安全的。对调用者来说，需要考虑底层的所以结构，接口的行为是不透明的。
//参考http://www.mongoing.com/archives/1462，注意3.6代码结构有很大的变化
//btree_key_generator.h[cpp] 该对象封装了一套解析解析算法，目的是解析出obj中的索引key

////IndexCatalogEntryImpl._accessMethod为该类型
//IndexCatalogImpl::_setupInMemoryStructures->KVDatabaseCatalogEntry::getIndex中确定使用那种method
class IndexAccessMethod { //最终初始化赋值在IndexAccessMethod* KVDatabaseCatalogEntry::getIndex
    MONGO_DISALLOW_COPYING(IndexAccessMethod);

public:
    IndexAccessMethod(IndexCatalogEntry* btreeState, SortedDataInterface* btree);
    virtual ~IndexAccessMethod() {}

    //
    // Lookup, traversal, and mutation support
    //

    /**
     * Internally generate the keys {k1, ..., kn} for 'obj'.  For each key k, insert (k ->
     * 'loc') into the index.  'obj' is the object at the location 'loc'.
     * 'numInserted' will be set to the number of keys added to the index for the document.  If
     * there is more than one key for 'obj', either all keys will be inserted or none will.
     *
     * The behavior of the insertion can be specified through 'options'.
     */
    /*
    通过getKeys获得doc的所有key，然后进行迭代修改Index，修改成功后，会对numInserted++，计数，统计本次操
    作修改索引次数。如果遇到失败情况，则要清理所有之前已经写入的数据，保证操作的原子性，即使失败，也要
    保证恢复到操作之前的数据状态（索引结构可能会发生变化，但是数据集合是等价的）。 另外，background build index，
    可能会重复插入索引，因为doc数据可能会在磁盘上移动，也就是会被重复扫描到。
    */
    Status insert(OperationContext* opCtx,
                  const BSONObj& obj,
                  const RecordId& loc,
                  const InsertDeleteOptions& options,
                  int64_t* numInserted);

    /**
     * Analogous to above, but remove the records instead of inserting them.
     * 'numDeleted' will be set to the number of keys removed from the index for the document.
     */
    Status remove(OperationContext* opCtx,
                  const BSONObj& obj,
                  const RecordId& loc,
                  const InsertDeleteOptions& options,
                  int64_t* numDeleted);

    /**
     * Checks whether the index entries for the document 'from', which is placed at location
     * 'loc' on disk, can be changed to the index entries for the doc 'to'. Provides a ticket
     * for actually performing the update.
     *
     * Returns an error if the update is invalid.  The ticket will also be marked as invalid.
     * Returns OK if the update should proceed without error.  The ticket is marked as valid.
     *
     * There is no obligation to perform the update after performing validation.
     */
    Status validateUpdate(OperationContext* opCtx,
                          const BSONObj& from,
                          const BSONObj& to,
                          const RecordId& loc,
                          const InsertDeleteOptions& options,
                          UpdateTicket* ticket,
                          const MatchExpression* indexFilter);

    /**
     * Perform a validated update.  The keys for the 'from' object will be removed, and the keys
     * for the object 'to' will be added.  Returns OK if the update succeeded, failure if it did
     * not.  If an update does not succeed, the index will be unmodified, and the keys for
     * 'from' will remain.  Assumes that the index has not changed since validateUpdate was
     * called.  If the index was changed, we may return an error, as our ticket may have been
     * invalidated.
     *
     * 'numInserted' will be set to the number of keys inserted into the index for the document.
     * 'numDeleted' will be set to the number of keys removed from the index for the document.
     * 对比先后两个对象，计算出diff，包括需要删除的和需要增加的，然后批量提交修改。注意，这里可能会失败，
     但是失败后索引没有再回滚到之前的数据集合。并且是先删除后增加，极端情况下会导致索引错误。思考：如果
     是先增加后删除，是不是更合适一点？
     */
    Status update(OperationContext* opCtx,
                  const UpdateTicket& ticket,
                  int64_t* numInserted,
                  int64_t* numDeleted);

    /**
     * Returns an unpositioned cursor over 'this' index.
     */
    std::unique_ptr<SortedDataInterface::Cursor> newCursor(OperationContext* opCtx,
                                                           bool isForward = true) const;
    /**
     * Returns a pseudo-random cursor over 'this' index.
     */
    std::unique_ptr<SortedDataInterface::Cursor> newRandomCursor(OperationContext* opCtx) const;

    // ------ index level operations ------


    /**
     * initializes this index
     * only called once for the lifetime of the index
     * if called multiple times, is an error
     */
    Status initializeAsEmpty(OperationContext* opCtx);

    /**
     * Try to page-in the pages that contain the keys generated from 'obj'.
     * This can be used to speed up future accesses to an index by trying to ensure the
     * appropriate pages are not swapped out.
     * See prefetch.cpp.
     */
    Status touch(OperationContext* opCtx, const BSONObj& obj);

    /**
     * this pages in the entire index
     */
    Status touch(OperationContext* opCtx) const;

    /**
     * Walk the entire index, checking the internal structure for consistency.
     * Set numKeys to the number of keys in the index.
     */
    void validate(OperationContext* opCtx, int64_t* numKeys, ValidateResults* fullResults);

    /**
     * Add custom statistics about this index to BSON object builder, for display.
     *
     * 'scale' is a scaling factor to apply to all byte statistics.
     *
     * Returns true if stats were appended.
     */
    bool appendCustomStats(OperationContext* opCtx, BSONObjBuilder* result, double scale) const;

    /**
     * @return The number of bytes consumed by this index.
     *         Exactly what is counted is not defined based on padding, re-use, etc...
     */
    long long getSpaceUsedBytes(OperationContext* opCtx) const;

    RecordId findSingle(OperationContext* opCtx, const BSONObj& key) const;

    /**
     * Attempt compaction to regain disk space if the indexed record store supports
     * compaction-in-place.
     */
    Status compact(OperationContext* opCtx);

    //
    // Bulk operations support
    //

    //IndexAccessMethod::initiateBulk中初始化
    class BulkBuilder {
    public:
        /**
         * Insert into the BulkBuilder as-if inserting into an IndexAccessMethod.
         */
        Status insert(OperationContext* opCtx,
                      const BSONObj& obj,
                      const RecordId& loc,
                      const InsertDeleteOptions& options,
                      int64_t* numInserted);

    private:
        friend class IndexAccessMethod;

        using Sorter = mongo::Sorter<BSONObj, RecordId>;

        BulkBuilder(const IndexAccessMethod* index,
                    const IndexDescriptor* descriptor,
                    size_t maxMemoryUsageBytes);

        //排序相关，默认为NoLimitSorter，不限制排序使用的内存
        std::unique_ptr<Sorter> _sorter;
        const IndexAccessMethod* _real;
        
        int64_t _keysInserted = 0;

        // Set to true if at least one document causes IndexAccessMethod::getKeys() to return a
        // BSONObjSet with size strictly greater than one.
        bool _everGeneratedMultipleKeys = false;

        // Holds the path components that cause this index to be multikey. The '_indexMultikeyPaths'
        // vector remains empty if this index doesn't support path-level multikey tracking.
        MultikeyPaths _indexMultikeyPaths;
    };

    /**
     * Starts a bulk operation.
     * You work on the returned BulkBuilder and then call commitBulk.
     * This can return NULL, meaning bulk mode is not available.
     *
     * It is only legal to initiate bulk when the index is new and empty.
     *
     * maxMemoryUsageBytes: amount of memory consumed before the external sorter starts spilling to
     *                      disk
     */
    std::unique_ptr<BulkBuilder> initiateBulk(size_t maxMemoryUsageBytes);

    /**
     * Call this when you are ready to finish your bulk work.
     * Pass in the BulkBuilder returned from initiateBulk.
     * @param bulk - something created from initiateBulk
     * @param mayInterrupt - is this commit interruptable (will cancel)
     * @param dupsAllowed - if false, error or fill 'dups' if any duplicate values are found
     * @param dups - if NULL, error out on dups if not allowed
     *               if not NULL, put the bad RecordIds there
     */
    Status commitBulk(OperationContext* opCtx,
                      std::unique_ptr<BulkBuilder> bulk,
                      bool mayInterrupt,
                      bool dupsAllowed,
                      std::set<RecordId>* dups);

    /**
     * Specifies whether getKeys should relax the index constraints or not.
     */
    enum class GetKeysMode { kRelaxConstraints, kEnforceConstraints };

    /**
     * Fills 'keys' with the keys that should be generated for 'obj' on this index.
     * Based on 'mode', it will honor or ignore index constraints, e.g. duplicated key, key too
     * long, and geo index parsing errors. The ignoring of constraints is for replication due to
     * idempotency reasons. In those cases, the generated 'keys' will be empty.
     *
     * If the 'multikeyPaths' pointer is non-null, then it must point to an empty vector. If this
     * index type supports tracking path-level multikey information, then this function resizes
     * 'multikeyPaths' to have the same number of elements as the index key pattern and fills each
     * element with the prefixes of the indexed field that would cause this index to be multikey as
     * a result of inserting 'keys'.
     */
    void getKeys(const BSONObj& obj,
                 GetKeysMode mode,
                 BSONObjSet* keys,
                 MultikeyPaths* multikeyPaths) const;

    /**
     * Splits the sets 'left' and 'right' into two vectors, the first containing the elements that
     * only appeared in 'left', and the second containing only elements that appeared in 'right'.
     *
     * Note this considers objects which are not identical as distinct objects. For example,
     * setDifference({BSON("a" << 0.0)}, {BSON("a" << 0LL)}) would result in the pair
     * ( {BSON("a" << 0.0)}, {BSON("a" << 0LL)} ).
     */
    static std::pair<std::vector<BSONObj>, std::vector<BSONObj>> setDifference(
        const BSONObjSet& left, const BSONObjSet& right);

protected:
    /**
     * Fills 'keys' with the keys that should be generated for 'obj' on this index.
     *
     * If the 'multikeyPaths' pointer is non-null, then it must point to an empty vector. If this
     * index type supports tracking path-level multikey information, then this function resizes
     * 'multikeyPaths' to have the same number of elements as the index key pattern and fills each
     * element with the prefixes of the indexed field that would cause this index to be multikey as
     * a result of inserting 'keys'.
     */
    virtual void doGetKeys(const BSONObj& obj,
                           BSONObjSet* keys,
                           MultikeyPaths* multikeyPaths) const = 0;

    /**
     * Determines whether it's OK to ignore ErrorCodes::KeyTooLong for this OperationContext
     */
    bool ignoreKeyTooLong(OperationContext* opCtx);

    //KVDatabaseCatalogEntry::getIndex中初始化，对应IndexCatalogEntryImpl
    IndexCatalogEntry* _btreeState;  // owned by IndexCatalogEntry
    const IndexDescriptor* _descriptor;

private:
    void removeOneKey(OperationContext* opCtx,
                      const BSONObj& key,
                      const RecordId& loc,
                      bool dupsAllowed);

    //IndexAccessMethod::IndexAccessMethod中初始化赋值，
    //wiredtiger存储引擎对应WiredTigerIndexUnique
    //唯一索引WiredTigerIndexUnique    普通索引WiredTigerIndexStandard
    const std::unique_ptr<SortedDataInterface> _newInterface;
};

/**
 * Updates are two steps: verify that it's a valid update, and perform it.
 * validateUpdate fills out the UpdateStatus and update actually applies it.
 */
class UpdateTicket {
public:
    UpdateTicket()
        : oldKeys(SimpleBSONObjComparator::kInstance.makeBSONObjSet()), newKeys(oldKeys) {}

private:
    friend class IndexAccessMethod;

    bool _isValid;

    BSONObjSet oldKeys;
    BSONObjSet newKeys;

    std::vector<BSONObj> removed;
    std::vector<BSONObj> added;

    RecordId loc;
    bool dupsAllowed;

    // Holds the path components that would cause this index to be multikey as a result of inserting
    // 'newKeys'. The 'newMultikeyPaths' vector remains empty if this index doesn't support
    // path-level multikey tracking.
    MultikeyPaths newMultikeyPaths;
};

/**
 * Flags we can set for inserts and deletes (and updates, which are kind of both).
 */
struct InsertDeleteOptions {
    // If there's an error, log() it.
    bool logIfError = false;

    // Are duplicate keys allowed in the index?
    //是否允许重复key
    //赋值参考IndexCatalogImpl::prepareInsertDeleteOptions
    bool dupsAllowed = false;

    // Should we relax the index constraints?
    IndexAccessMethod::GetKeysMode getKeysMode =
        IndexAccessMethod::GetKeysMode::kEnforceConstraints;
};

}  // namespace mongo
