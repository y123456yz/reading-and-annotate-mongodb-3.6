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

#include <cstdint>
#include <limits>
#include <string>

#include "mongo/base/static_assert.h"
#include "mongo/base/string_data.h"
#include "mongo/config.h"
#include "mongo/platform/hash_namespace.h"

namespace mongo {

class Locker;

struct LockHead;
struct PartitionedLockHead;

/*
MongoDB 加锁时，有四种模式【MODE_IS、MODE_IX、MODE_S、MODE_X】，MODE_S， MODE_X 很容易理解，分别是互斥读锁、
互斥写锁，MODE_IS、MODE_IX是为了实现层次锁模型引入的，称为意向读锁、意向写锁，锁之间的竞争情况如上图所示。

MongoDB在加锁时，是一个层次性的管理方式，从 globalLock ==> DBLock ==> CollecitonLock … ，比如我们都知道
MongoDB wiredtiger是文档级别锁，那么读写并发时，加锁就类似如下

写操作

1. globalLock  (这一层只关注是读还是写，不关注具体是什么LOCK)
2. DBLock MODE_IX
3. Colleciotn MODE_IX
4. pass request to wiredtiger

读操作
1. globalLock MODE_IS  (这一层只关注是读还是写，不关注具体是什么LOCK)
2. DBLock MODE_IS
3. Colleciton MODE_IS
4. pass request to wiredtiger
根据上图的竞争情况，IS和IX是无需竞争的，所以读写请求可以在没有竞争的情况下，同时传到wiredtiger引擎去处理。

再举个栗子，如果一个前台建索引的操作跟一个读请求并发了

前台建索引操作

1. globalLock MODE_IX (这一层只关注是读还是写，不关注具体是什么LOCK)
2. DBLock MODE_X
3. pass to wiredtiger

读操作
1. globalLock MODE_IS (这一层只关注是读还是写，不关注具体是什么LOCK)
2. DBLock MODE_IS
3. Colleciton MODE_IS
4. pass request to wiredtiger
根据竞争表，MODE_X和MODE_IS是要竞争的，这也就是为什么前台建索引的过程中读是被阻塞的。

我们今天介绍的 globalLock 对应上述的第一步，在globalLock这一层，只关心是读锁、还是写锁，不关心是互斥锁还是意向锁，
所以 globalLock 这一层是不存在竞争的。
http://www.mongoing.com/archives/4768

所有的锁都是平等的，它们是排在一个队列里，符合FIFO原则。但是，MongoDB做了优化，即当一个锁被采用时，
所有与它兼容的锁（即上表为yes的锁）都会被采纳，从而可以并发操作。举个例子，当你针对Collection A中
的Document a使用S锁时，其它reader可以同时使用S锁来读取该Document a，也可以同时读取同一个Collection
的Document b.因为所有的S锁都是兼容的。那么，如果此时针对Collection A中的Document c进行写操作是否可
以呢？显然需要为Document c赋予x锁，此时Collection A就需要IX锁，而由于IX和IS是兼容的，所以没有问题。
简单来说，只要不是同一个Document，读写操作是可以并发的；如果是同一个Document，读可以并发，但写不可以。
https://www.jianshu.com/p/d838a5905303
*/
/**
 * Lock modes.
 *
 * Compatibility Matrix  相容性关系 +相容共存  
 *                                          Granted mode
 *   ---------------.--------------------------------------------------------.
 *   Requested Mode | MODE_NONE  MODE_IS   MODE_IX  MODE_S   MODE_X  |
 *     MODE_IS      |      +        +         +        +        -    |
 *     MODE_IX      |      +        +         +        -        -    |
 *     MODE_S       |      +        +         -        +        -    |
 *     MODE_X       |      +        -         -        -        -    |
 */
enum LockMode {
    MODE_NONE = 0,
    MODE_IS = 1,
    MODE_IX = 2,
    MODE_S = 3,
    MODE_X = 4,

    // Counts the lock modes. Used for array size allocations, etc. Always insert new lock
    // modes above this entry.
    LockModesCount
};

/**
 * Returns a human-readable name for the specified lock mode.
 */
const char* modeName(LockMode mode);

/**
 * Legacy lock mode names in parity for 2.6 reports.
 */
const char* legacyModeName(LockMode mode);

/**
 * Mode A is covered by mode B if the set of conflicts for mode A is a subset of the set of
 * conflicts for mode B. For example S is covered by X. IS is covered by S. However, IX is not
 * covered by S or IS.
 */
bool isModeCovered(LockMode mode, LockMode coveringMode);

/**
 * Returns whether the passed in mode is S or IS. Used for validation checks.
 */
inline bool isSharedLockMode(LockMode mode) {
    return (mode == MODE_IS || mode == MODE_S);
}


/**
 * Return values for the locking functions of the lock manager.
 */
enum LockResult {

    /**
     * The lock request was granted and is now on the granted list for the specified resource.
     */
    LOCK_OK,

    /**
     * The lock request was not granted because of conflict. If this value is returned, the
     * request was placed on the conflict queue of the specified resource and a call to the
     * LockGrantNotification::notify callback should be expected with the resource whose lock
     * was requested.
     */
    LOCK_WAITING,

    /**
     * The lock request waited, but timed out before it could be granted. This value is never
     * returned by the LockManager methods here, but by the Locker class, which offers
     * capability to block while waiting for locks.
     */
    LOCK_TIMEOUT,

    /**
     * The lock request was not granted because it would result in a deadlock. No changes to
     * the state of the Locker would be made if this value is returned (i.e., it will not be
     * killed due to deadlock). It is up to the caller to decide how to recover from this
     * return value - could be either release some locks and try again, or just bail with an
     * error and have some upper code handle it.
     */
    LOCK_DEADLOCK,

    /**
     * This is used as an initialiser value. Should never be returned.
     */
    LOCK_INVALID
};


/**
 * Hierarchy of resource types. The lock manager knows nothing about this hierarchy, it is
 * purely logical. Resources of different types will never conflict with each other.
 *
 * While the lock manager does not know or care about ordering, the general policy is that
 * resources are acquired in the order below. For example, one might first acquire a
 * RESOURCE_GLOBAL and then the desired RESOURCE_DATABASE, both using intent modes, and
 * finally a RESOURCE_COLLECTION in exclusive mode. When locking multiple resources of the
 * same type, the canonical order is by resourceId order.
 *
 * It is OK to lock resources out of order, but it is the users responsibility to ensure
 * ordering is consistent so deadlock cannot occur.
 */
enum ResourceType {
    // Types used for special resources, use with a hash id from ResourceId::SingletonHashIds.
    RESOURCE_INVALID = 0,
    RESOURCE_GLOBAL,        // Used for mode changes or global exclusive operations
    RESOURCE_MMAPV1_FLUSH,  // Necessary only for the MMAPv1 engine

    // Generic resources, used for multi-granularity locking, together with RESOURCE_GLOBAL
    RESOURCE_DATABASE,
    RESOURCE_COLLECTION,
    RESOURCE_METADATA,

    // Resource type used for locking general resources not related to the storage hierarchy.
    RESOURCE_MUTEX,

    // Counts the rest. Always insert new resource types above this entry.
    ResourceTypesCount
};

/**
 * Returns a human-readable name for the specified resource type.
 */
const char* resourceTypeName(ResourceType resourceType);

/**
 * Uniquely identifies a lockable resource.
 */
class ResourceId {
    // We only use 3 bits for the resource type in the ResourceId hash
    enum { resourceTypeBits = 3 };
    MONGO_STATIC_ASSERT(ResourceTypesCount <= (1 << resourceTypeBits));

public:
    /**
     * Assign hash ids for special resources to avoid accidental reuse of ids. For ids used
     * with the same ResourceType, the order here must be the same as the locking order.
     */
    enum SingletonHashIds {
        SINGLETON_INVALID = 0,
        SINGLETON_PARALLEL_BATCH_WRITER_MODE,
        SINGLETON_GLOBAL,
        SINGLETON_MMAPV1_FLUSH,
    };

    ResourceId() : _fullHash(0) {}
    ResourceId(ResourceType type, StringData ns);
    ResourceId(ResourceType type, const std::string& ns);
    ResourceId(ResourceType type, uint64_t hashId);

    bool isValid() const {
        return getType() != RESOURCE_INVALID;
    }

    operator uint64_t() const {
        return _fullHash;
    }

    // This defines the canonical locking order, first by type and then hash id
    bool operator<(const ResourceId& rhs) const {
        return _fullHash < rhs._fullHash;
    }

    ResourceType getType() const {
        return static_cast<ResourceType>(_fullHash >> (64 - resourceTypeBits));
    }

    uint64_t getHashId() const {
        return _fullHash & (std::numeric_limits<uint64_t>::max() >> resourceTypeBits);
    }

    std::string toString() const;

private:
    /**
     * The top 'resourceTypeBits' bits of '_fullHash' represent the resource type,
     * while the remaining bits contain the bottom bits of the hashId. This avoids false
     * conflicts between resources of different types, which is necessary to prevent deadlocks.
     */
    uint64_t _fullHash;

    static uint64_t fullHash(ResourceType type, uint64_t hashId);

#ifdef MONGO_CONFIG_DEBUG_BUILD
    // Keep the complete namespace name for debugging purposes (TODO: this will be
    // removed once we are confident in the robustness of the lock manager).
    std::string _nsCopy;
#endif
};

#ifndef MONGO_CONFIG_DEBUG_BUILD
// Treat the resource ids as 64-bit integers in release mode in order to ensure we do
// not spend too much time doing comparisons for hashing.
MONGO_STATIC_ASSERT(sizeof(ResourceId) == sizeof(uint64_t));
#endif


// Type to uniquely identify a given locker object
typedef uint64_t LockerId;

// Hardcoded resource id for the oplog collection, which is special-cased both for resource
// acquisition purposes and for statistics reporting.
extern const ResourceId resourceIdLocalDB;
extern const ResourceId resourceIdOplog;

// Hardcoded resource id for admin db. This is to ensure direct writes to auth collections
// are serialized (see SERVER-16092)
extern const ResourceId resourceIdAdminDB;

// Hardcoded resource id for ParallelBatchWriterMode. We use the same resource type
// as resourceIdGlobal. This will also ensure the waits are reported as global, which
// is appropriate. The lock will never be contended unless the parallel batch writers
// must stop all other accesses globally. This resource must be locked before all other
// resources (including resourceIdGlobal). Replication applier threads don't take this
// lock.
// TODO: Merge this with resourceIdGlobal
extern const ResourceId resourceIdParallelBatchWriterMode;

/**
 * Interface on which granted lock requests will be notified. See the contract for the notify
 * method for more information and also the LockManager::lock call.
 *
 * The default implementation of this method would simply block on an event until notify has
 * been invoked (see CondVarLockGrantNotification).
 *
 * Test implementations could just count the number of notifications and their outcome so that
 * they can validate locks are granted as desired and drive the test execution.
 */
class LockGrantNotification {
public:
    virtual ~LockGrantNotification() {}

    /**
     * This method is invoked at most once for each lock request and indicates the outcome of
     * the lock acquisition for the specified resource id.
     *
     * Cases where it won't be called are if a lock acquisition (be it in waiting or converting
     * state) is cancelled through a call to unlock.
     *
     * IMPORTANT: This callback runs under a spinlock for the lock manager, so the work done
     *            inside must be kept to a minimum and no locks or operations which may block
     *            should be run. Also, no methods which call back into the lock manager should
     *            be invoked from within this methods (LockManager is not reentrant).
     *
     * @resId ResourceId for which a lock operation was previously called.
     * @result Outcome of the lock operation.
     */
    virtual void notify(ResourceId resId, LockResult result) = 0;
};


/**
 * There is one of those entries per each request for a lock. They hang on a linked list off
 * the LockHead or off a PartitionedLockHead and also are in a map for each Locker. This
 * structure is not thread-safe.
 *
 * LockRequest are owned by the Locker class and it controls their lifetime. They should not
 * be deleted while on the LockManager though (see the contract for the lock/unlock methods).
 */
struct LockRequest {
    enum Status {
        STATUS_NEW,
        STATUS_GRANTED,
        STATUS_WAITING,
        STATUS_CONVERTING,

        // Counts the rest. Always insert new status types above this entry.
        StatusCount
    };

    /**
     * Used for initialization of a LockRequest, which might have been retrieved from cache.
     */
    void initNew(Locker* locker, LockGrantNotification* notify);

    // This is the Locker, which created this LockRequest. Pointer is not owned, just referenced.
    // Must outlive the LockRequest.
    //
    // Written at construction time by Locker
    // Read by LockManager on any thread
    // No synchronization
    Locker* locker;

    // Notification to be invoked when the lock is granted. Pointer is not owned, just referenced.
    // If a request is in the WAITING or CONVERTING state, must live at least until
    // LockManager::unlock is cancelled or the notification has been invoked.
    //
    // Written at construction time by Locker
    // Read by LockManager
    // No synchronization
    LockGrantNotification* notify;

    // If the request cannot be granted right away, whether to put it at the front or at the end of
    // the queue. By default, requests are put at the back. If a request is requested to be put at
    // the front, this effectively bypasses fairness. Default is FALSE.
    //
    // Written at construction time by Locker
    // Read by LockManager on any thread
    // No synchronization
    bool enqueueAtFront;

    // When this request is granted and as long as it is on the granted queue, the particular
    // resource's policy will be changed to "compatibleFirst". This means that even if there are
    // pending requests on the conflict queue, if a compatible request comes in it will be granted
    // immediately. This effectively turns off fairness.
    //
    // Written at construction time by Locker
    // Read by LockManager on any thread
    // No synchronization
    bool compatibleFirst;

    // When set, an attempt is made to execute this request using partitioned lockheads. This speeds
    // up the common case where all requested locking modes are compatible with each other, at the
    // cost of extra overhead for conflicting modes.
    //
    // Written at construction time by LockManager
    // Read by LockManager on any thread
    // No synchronization
    bool partitioned;

    // How many times has LockManager::lock been called for this request. Locks are released when
    // their recursive count drops to zero.
    //
    // Written by LockManager on Locker thread
    // Read by LockManager on Locker thread
    // Read by Locker on Locker thread
    // No synchronization
    unsigned recursiveCount;

    // Pointer to the lock to which this request belongs, or null if this request has not yet been
    // assigned to a lock or if it belongs to the PartitionedLockHead for locker (in which case
    // partitionedLock must be set). The LockHead should be alive as long as there are LockRequests
    // on it, so it is safe to have this pointer hanging around.
    //
    // Written by LockManager on any thread
    // Read by LockManager on any thread
    // Protected by LockHead bucket's mutex
    LockHead* lock;

    // Pointer to the partitioned lock to which this request belongs, or null if it is not
    // partitioned. Only one of 'lock' and 'partitionedLock' is non-NULL, and a request can only
    // transition from 'partitionedLock' to 'lock', never the other way around.
    //
    // Written by LockManager on any thread
    // Read by LockManager on any thread
    // Protected by LockHead bucket's mutex
    PartitionedLockHead* partitionedLock;

    // The linked list chain on which this request hangs off the owning lock head. The reason
    // intrusive linked list is used instead of the std::list class is to allow for entries to be
    // removed from the middle of the list in O(1) time, if they are known instead of having to
    // search for them and we cannot persist iterators, because the list can be modified while an
    // iterator is held.
    //
    // Written by LockManager on any thread
    // Read by LockManager on any thread
    // Protected by LockHead bucket's mutex
    LockRequest* prev;
    LockRequest* next;

    // The current status of this request. Always starts at STATUS_NEW.
    //
    // Written by LockManager on any thread
    // Read by LockManager on any thread
    // Protected by LockHead bucket's mutex
    Status status;

    // If this request is not granted, the mode which has been requested for this lock. If granted,
    // the mode in which it is currently granted.
    //
    // Written by LockManager on any thread
    // Read by LockManager on any thread
    // Protected by LockHead bucket's mutex
    LockMode mode;

    // This value is different from MODE_NONE only if a conversion is requested for a lock and that
    // conversion cannot be immediately granted.
    //
    // Written by LockManager on any thread
    // Read by LockManager on any thread
    // Protected by LockHead bucket's mutex
    LockMode convertMode;
};

/**
 * Returns a human readable status name for the specified LockRequest status.
 */
const char* lockRequestStatusName(LockRequest::Status status);

}  // namespace mongo


MONGO_HASH_NAMESPACE_START
template <>
struct hash<mongo::ResourceId> {
    size_t operator()(const mongo::ResourceId& resource) const {
        return resource;
    }
};
MONGO_HASH_NAMESPACE_END
