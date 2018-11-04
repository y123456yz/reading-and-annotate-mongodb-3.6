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

#include <vector>

#include "mongo/base/disallow_copying.h"
#include "mongo/db/keys_collection_manager.h"
#include "mongo/db/logical_session_id.h"
#include "mongo/db/storage/storage_engine.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/platform/unordered_set.h"
#include "mongo/stdx/condition_variable.h"
#include "mongo/stdx/functional.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/mutex.h"
#include "mongo/transport/service_executor.h"
#include "mongo/transport/session.h"
#include "mongo/util/clock_source.h"
#include "mongo/util/decorable.h"
#include "mongo/util/periodic_runner.h"
#include "mongo/util/tick_source.h"

namespace mongo {

class AbstractMessagingPort;
class Client;
class OperationContext;
class OpObserver;
class ServiceEntryPoint;

namespace transport {
class TransportLayer;
}  // namespace transport

/**
 * Classes that implement this interface can receive notification on killOp.
 *
 * See registerKillOpListener() for more information,
 * including limitations on the lifetime of registered listeners.
 */
class KillOpListenerInterface {
    MONGO_DISALLOW_COPYING(KillOpListenerInterface);

public:
    /**
     * Will be called *after* ops have been told they should die.
     * Callback must not fail.
     */
    virtual void interrupt(unsigned opId) = 0;
    virtual void interruptAll() = 0;

protected:
    KillOpListenerInterface() = default;

    // Should not delete through a pointer of this type
    virtual ~KillOpListenerInterface() = default;
};

//StorageFactoriesIteratorMongoD继承该类
//上面的ServiceContextMongoD:makeStorageFactoriesIterator 使用到该类做遍历
class StorageFactoriesIterator {
    MONGO_DISALLOW_COPYING(StorageFactoriesIterator);

public:
    virtual ~StorageFactoriesIterator() = default;

    virtual bool more() const = 0;
    virtual const StorageEngine::Factory* next() = 0;

protected:
    StorageFactoriesIterator() = default;
};

/**
 * Class representing the context of a service, such as a MongoD database service or
 * a MongoS routing service.
 *
 * A ServiceContext is the root of a hierarchy of contexts.  A ServiceContext owns
 * zero or more Clients, which in turn each own OperationContexts.
 ServiceContext->Clients->OperationContexts
 */
 
//ServiceContextMongoD继承ServiceContext，ServiceContextMongoD包含生成OperationContext类的接口
//ServiceContextMongoD->ServiceContext(包含ServiceEntryPoint成员)
//ServiceEntryPointMongod->ServiceEntryPointImpl->ServiceEntryPoint

//ServiceContextMongoD->ServiceContext   
//ServiceContext包含OperationContext成员，见UniqueOperationContext
class ServiceContext : public Decorable<ServiceContext> {
    MONGO_DISALLOW_COPYING(ServiceContext);

public:
    /**
     * Special deleter used for cleaning up Client objects owned by a ServiceContext.
     * See UniqueClient, below.
     */ //后面的UniqueClient会用到，用来清理Client对象
    class ClientDeleter { //下面的UniqueClient用到该deleter
    public:
        //ServiceContext::ClientDeleter::operator()
        void operator()(Client* client) const;
    };

    /**
     * Observer interface implemented to hook client and operation context creation and
     * destruction.
     */ //AuthzClientObserver继承该类    registerClientObserver
    class ClientObserver { //客户端操作接口
    public:
        virtual ~ClientObserver() = default;

        /**
         * Hook called after a new client "client" is created on a service by
         * service->makeClient().  ServiceContext::makeClient中调用
         *
         * For a given client and registered instance of ClientObserver, if onCreateClient
         * returns without throwing an exception, onDestroyClient will be called when "client"
         * is deleted.
         */ //makeClient中调用, 消耗接口为onDestroyClient，onCreateClient和onDestroyClient对应
        virtual void onCreateClient(Client* client) = 0;

        /**
         * Hook called on a "client" created by a service before deleting "client".
         *
         * Like a destructor, must not throw exceptions.
         */ //onCreateClient和onDestroyClient对应
        virtual void onDestroyClient(Client* client) = 0;

        /**
         * Hook called after a new operation context is created on a client by
         * service->makeOperationContext(client)  or client->makeOperationContext().
         *
         * For a given operation context and registered instance of ClientObserver, if
         * onCreateOperationContext returns without throwing an exception,
         * onDestroyOperationContext will be called when "opCtx" is deleted.
         */ //service->makeOperationContext(client)  or client->makeOperationContext().
        virtual void onCreateOperationContext(OperationContext* opCtx) = 0;

        /**
         * Hook called on a "opCtx" created by a service before deleting "opCtx".
         *
         * Like a destructor, must not throw exceptions.
         */
        virtual void onDestroyOperationContext(OperationContext* opCtx) = 0;
    };

    using ClientSet = unordered_set<Client*>;

    /**
     * Cursor for enumerating the live Client objects belonging to a ServiceContext.
     *
     * Lifetimes of this type are synchronized with client creation and destruction.
     */ //记录属于某个ServiceContext的在线客户端的游标信息，这种类型的生命周期与客户端创建和销毁同步
    class LockedClientsCursor {
    public:
        /**
         * Constructs a cursor for enumerating the clients of "service", blocking "service" from
         * creating or destroying Client objects until this instance is destroyed.
         */ //构造一个客户端游标锁，锁住service，避免在这个过程中创建和消耗client对象
        explicit LockedClientsCursor(ServiceContext* service);

        /**
         * Returns the next client in the enumeration, or nullptr if there are no more clients.
         */ //获取下一个客户端
        Client* next();

    private:
        stdx::unique_lock<stdx::mutex> _lock;
        ClientSet::const_iterator _curr;
        ClientSet::const_iterator _end;
    };

    /**
     * Special deleter used for cleaning up OperationContext objects owned by a ServiceContext.
     * See UniqueOperationContext, below.
     */ //删除OperationContext，见ServiceContext::OperationContextDeleter::operator()
    class OperationContextDeleter { //下面的UniqueOperationContext用到
    public:
        void operator()(OperationContext* opCtx) const;
    };

    /**
     * This is the unique handle type for Clients created by a ServiceContext.
     */ //ServiceContext::UniqueClient  ServiceContext::makeClient中赋值
    using UniqueClient = std::unique_ptr<Client, ClientDeleter>;

    /**
     * This is the unique handle type for OperationContexts created by a ServiceContext.
     */ //ServiceContext::makeOperationContext中赋值
    using UniqueOperationContext = std::unique_ptr<OperationContext, OperationContextDeleter>;

    virtual ~ServiceContext();

    /**
     * Registers an observer of lifecycle events on Clients created by this ServiceContext.
     *
     * See the ClientObserver type, above, for details.
     *
     * All calls to registerClientObserver must complete before ServiceContext
     * is used in multi-threaded operation, or is used to create clients via calls
     * to makeClient.
     */ //把一个ClientObserver存入到_clientObservers成员
    void registerClientObserver(std::unique_ptr<ClientObserver> observer);

    /**
     * Creates a new Client object representing a client session associated with this
     * ServiceContext.
     * 创建一个新的客户端对象，该对象与servicecontext关联
     * The "desc" string is used to set a descriptive name for the client, used in logging.
     * desc为该Client的描述信息
     * If supplied, "session" is the transport::Session used for communicating with the client.
     * session用来与客户端进行会话通信
     */
    UniqueClient makeClient(std::string desc, transport::SessionHandle session = nullptr);

    /**
     * Creates a new OperationContext on "client".
     *
     * "client" must not have an active operation context.
     *
     */ //构建OperationContext对象
    UniqueOperationContext makeOperationContext(Client* client);

    //
    // Storage
    //

    /**
     * Register a storage engine.  Called from a MONGO_INIT that depends on initializiation of
     * the global environment.
     * Ownership of 'factory' is transferred to global environment upon registration.
     */ //注册存储引擎，ServiceContextMongoD::registerStorageEngine
     //例如wiredtiger_init.cpp, getGlobalServiceContext()->registerStorageEngine(kWiredTigerEngineName, new WiredTigerFactory());
    virtual void registerStorageEngine(const std::string& name,
                                       const StorageEngine::Factory* factory) = 0;

    /**
     * Returns true if "name" refers to a registered storage engine.
     */
    virtual bool isRegisteredStorageEngine(const std::string& name) = 0;

    /**
     * Produce an iterator over all registered storage engine factories.
     * Caller owns the returned object and is responsible for deleting when finished.
     *
     * Never returns nullptr.
     */ 
    //存储引擎遍历器
    virtual StorageFactoriesIterator* makeStorageFactoriesIterator() = 0;
    //初始化存储引擎
    virtual void initializeGlobalStorageEngine() = 0;

    /**
     * Shuts down storage engine cleanly and releases any locks on mongod.lock.
     */ //关闭全局存储引擎
    virtual void shutdownGlobalStorageEngineCleanly() = 0;

    /**
     * Return the storage engine instance we're using.
     */ //获取我们正在使用的存储引擎
    virtual StorageEngine* getGlobalStorageEngine() = 0;

    //
    // Global operation management.  This may not belong here and there may be too many methods
    // here.
    //

    /*
    MongoDB提供了killOp请求，用于干掉运行时间很长的请求，killOp通常需要与currentOp组合起来使用；
    先根据currentOp查询到请求的opid，然后根据opid发送killOp的请求。
    */
    /**
     * Signal all OperationContext(s) that they have been killed.
     */ //向所有的已经被kill掉的OperationContext发送信号
    void setKillAllOperations();

    /**
     * Reset the operation kill state after a killAllOperations.
     * Used for testing.
     */
    void unsetKillAllOperations();

    /**
     * Get the state for killing all operations.
     */
    bool getKillAllOperations() {
        return _globalKill.loadRelaxed();
    }

    /**
     * Kills the operation "opCtx" with the code "killCode", if opCtx has not already been killed.
     * Caller must own the lock on opCtx->getClient, and opCtx->getServiceContext() must be the same
     *as
     * this service context.
     **/
    /*
    MongoDB提供了killOp请求，用于干掉运行时间很长的请求，killOp通常需要与currentOp组合起来使用；
    先根据currentOp查询到请求的opid，然后根据opid发送killOp的请求。
    */
    void killOperation(OperationContext* opCtx,
                       ErrorCodes::Error killCode = ErrorCodes::Interrupted);

    /**
     * Kills all operations that have a Client that is associated with an incoming user
     * connection, except for the one associated with opCtx.
     */
    void killAllUserOperations(const OperationContext* opCtx, ErrorCodes::Error killCode);

    /**
     * Registers a listener to be notified each time an op is killed.
     *
     * listener does not become owned by the environment. As there is currently no way to
     * unregister, the listener object must outlive this ServiceContext object.
     */
    void registerKillOpListener(KillOpListenerInterface* listener);

    //
    // Background tasks.
    //

    /**
     * Set a periodic runner on the service context. The runner should already be
     * started when it is moved onto the service context. The service context merely
     * takes ownership of this object to allow it to continue running for the life of
     * the process
     */
    void setPeriodicRunner(std::unique_ptr<PeriodicRunner> runner);

    /**
     * Returns a pointer to the global periodic runner owned by this service context.
     */  //获取一个本serviceContext实例拥有的全局PeriodicRunner
    PeriodicRunner* getPeriodicRunner() const;

    //
    // Transport.
    //

    /**
     * Get the master TransportLayer. Routes to all other TransportLayers that
     * may be in use within this service.
     *
     * See TransportLayerManager for more details.
     */
    transport::TransportLayer* getTransportLayer() const;

    /**
     * Get the service entry point for the service context.
     *
     * See ServiceEntryPoint for more details.
     */
    ServiceEntryPoint* getServiceEntryPoint() const;

    /**
     * Get the service executor for the service context.
     *
     * See ServiceStateMachine for how this is used. Some configurations may not have a service
     * executor registered and this will return a nullptr.
     */
    transport::ServiceExecutor* getServiceExecutor() const;

    /**
     * Waits for the ServiceContext to be fully initialized and for all TransportLayers to have been
     * added/started.
     *
     * If startup is already complete this returns immediately.
     */ //等待该ServiceContext的startup运行完成
    void waitForStartupComplete();

    /*
     * Marks initialization as complete and all transport layers as started.
     */
    void notifyStartupComplete();

    /**
     * Set the OpObserver.
     */
    void setOpObserver(std::unique_ptr<OpObserver> opObserver);

    /**
     * Return the OpObserver instance we're using.
     */
    OpObserver* getOpObserver() const {
        return _opObserver.get();
    }

    /**
     * Returns the tick/clock source set in this context.
     */
    TickSource* getTickSource() const {
        return _tickSource.get();
    }

    /**
     * Get a ClockSource implementation that may be less precise than the _preciseClockSource but
     * may be cheaper to call.
     */ //没_preciseClockSource精确
    ClockSource* getFastClockSource() const {
        return _fastClockSource.get();
    }

    /**
     * Get a ClockSource implementation that is very precise but may be expensive to call.
     */ //比较精确的clock
    ClockSource* getPreciseClockSource() const {
        return _preciseClockSource.get();
    }

    /**
     * Replaces the current tick/clock source with a new one. In other words, the old source will be
     * destroyed. So make sure that no one is using the old source when calling this.
     */
    void setTickSource(std::unique_ptr<TickSource> newSource);

    /**
     * Call this method with a ClockSource implementation that may be less precise than
     * the _preciseClockSource but may be cheaper to call.
     */
    void setFastClockSource(std::unique_ptr<ClockSource> newSource);

    /**
     * Call this method with a ClockSource implementation that is very precise but
     * may be expensive to call.
     */
    void setPreciseClockSource(std::unique_ptr<ClockSource> newSource);

    /**
     * Binds the service entry point implementation to the service context.
     */
    void setServiceEntryPoint(std::unique_ptr<ServiceEntryPoint> sep);

    /**
     * Binds the TransportLayer to the service context. The TransportLayer should have already
     * had setup() called successfully, but not startup().
     *
     * This should be a TransportLayerManager created with the global server configuration.
     */
    void setTransportLayer(std::unique_ptr<transport::TransportLayer> tl);

    /**
     * Binds the service executor to the service context
     */
    void setServiceExecutor(std::unique_ptr<transport::ServiceExecutor> exec);

protected:
    ServiceContext();

    /**
     * Mutex used to synchronize access to mutable state of this ServiceContext instance,
     * including possibly by its subclasses.
     */
    stdx::mutex _mutex;

private:
    /**
     * Returns a new OperationContext. Private, for use by makeOperationContext.
     */
    virtual std::unique_ptr<OperationContext> _newOpCtx(Client* client, unsigned opId) = 0;

    /**
     * The periodic runner. 
     */
    std::unique_ptr<PeriodicRunner> _runner;

    /**
     * The TransportLayer.  ServiceContext:_transportLayer
     */
    std::unique_ptr<transport::TransportLayer> _transportLayer;

    /**
     * The service entry point
     */ 
    std::unique_ptr<ServiceEntryPoint> _serviceEntryPoint;

    /**
     * The ServiceExecutor
     */
    /*
    "adaptive") : <ServiceExecutorAdaptive>( 引入了boost.asio库实现网络接口的异步调用并作为默认配置，同时还把线程模型调整
    为线程池，动态根据workload压力情况调整线程数量，在大量连接情况下可以避免产生大量的处理线程，降低线程切换开销，以获得
    更稳定的性能表现。
    
    "synchronous"): <ServiceExecutorSynchronous>(ctx));  每个网络连接创建一个专用线程，并同步调用网络接口recv/send收发包
    }
    */ //参考官方配置https://docs.mongodb.com/manual/reference/configuration-options/
    std::unique_ptr<transport::ServiceExecutor> _serviceExecutor;

    /**
     * Vector of registered observers.
     */ 
    //ServiceContext::registerClientObserver把一个ClientObserver存入到_clientObservers成员
    //ServiceContext::makeClient中遍历该vector
    std::vector<std::unique_ptr<ClientObserver>> _clientObservers;
    //ServiceContext::makeClient中插入
    ClientSet _clients;

    /**
     * The registered OpObserver.
     */
    std::unique_ptr<OpObserver> _opObserver;
    //注意_tickSource  _fastClockSource  _preciseClockSource的区别
    std::unique_ptr<TickSource> _tickSource;

    /**
     * A ClockSource implementation that may be less precise than the _preciseClockSource but
     * may be cheaper to call.
     */ 
    //setFastClockSource中赋值，_fastClockSource没_preciseClockSource精确
    std::unique_ptr<ClockSource> _fastClockSource;

    /**
     * A ClockSource implementation that is very precise but may be expensive to call.
     */ //_fastClockSource没_preciseClockSource精确
    std::unique_ptr<ClockSource> _preciseClockSource;

    // Flag set to indicate that all operations are to be interrupted ASAP.
    AtomicWord<bool> _globalKill{false};

    // protected by _mutex
    std::vector<KillOpListenerInterface*> _killOpListeners;

    // Counter for assigning operation ids.
    AtomicUInt32 _nextOpId{1};

    //等待startup完成相关的条件变量
    bool _startupComplete = false;
    stdx::condition_variable _startupCompleteCondVar;
};

/**
 * Returns true if there is a global ServiceContext.
 */
bool hasGlobalServiceContext();

/**
 * Returns the singleton ServiceContext for this server process.
 *
 * Fatal if there is currently no global ServiceContext.
 *
 * Caller does not own pointer.
 */
ServiceContext* getGlobalServiceContext();

/**
 * Warning - This function is temporary. Do not introduce new uses of this API.
 *
 * Returns the singleton ServiceContext for this server process.
 *
 * Waits until there is a valid global ServiceContext.
 *
 * Caller does not own pointer.
 */
ServiceContext* waitAndGetGlobalServiceContext();

/**
 * Sets the global ServiceContext.  If 'serviceContext' is NULL, un-sets and deletes
 * the current global ServiceContext.
 *
 * Takes ownership of 'serviceContext'.
 */
void setGlobalServiceContext(std::unique_ptr<ServiceContext>&& serviceContext);

/**
 * Shortcut for querying the storage engine about whether it supports document-level locking.
 * If this call becomes too expensive, we could cache the value somewhere so we don't have to
 * fetch the storage engine every time.
 */
bool supportsDocLocking();

/**
 * Returns true if the storage engine in use is MMAPV1.
 */
bool isMMAPV1();

/*
 * Extracts the storageEngine bson from the CollectionOptions provided.  Loops through each
 * provided storageEngine and asks the matching registered storage engine if the
 * collection/index options are valid.  Returns an error if the collection/index options are
 * invalid.
 * If no matching registered storage engine is found, return an error.
 * Validation function 'func' must be either:
 * - &StorageEngine::Factory::validateCollectionStorageOptions; or
 * - &StorageEngine::Factory::validateIndexStorageOptions
 */
Status validateStorageOptions(
    const BSONObj& storageEngineOptions,
    stdx::function<Status(const StorageEngine::Factory* const, const BSONObj&)> validateFunc);

/*
 * Returns a BSONArray containing the names of available storage engines, or an empty
 * array if there is no global ServiceContext
 */
BSONArray storageEngineList();

/*
 * Appends a the list of available storage engines to a BSONObjBuilder for reporting purposes.
 */
void appendStorageEngineList(BSONObjBuilder* result);

}  // namespace mongo
