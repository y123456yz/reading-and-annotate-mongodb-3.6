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

#include <atomic>

#include "mongo/base/status.h"
#include "mongo/config.h"
#include "mongo/db/service_context.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/functional.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/mutex.h"
#include "mongo/stdx/thread.h"
#include "mongo/transport/message_compressor_base.h"
#include "mongo/transport/service_entry_point.h"
#include "mongo/transport/service_executor.h"
#include "mongo/transport/session.h"
#include "mongo/transport/transport_mode.h"

namespace mongo {

/*
 * The ServiceStateMachine holds the state of a single client connection and represents the
 * lifecycle of each user request as a state machine. It is the glue between the stateless
 * ServiceEntryPoint and TransportLayer that ties network and database logic together for a
 * user.

 ServiceStateMachine保存单个客户端连接的状态，并将每个用户请求的生命周期表示为一个状态机。
 它是无状态的ServiceEntryPoint和TransportLayer之间的桥梁，为用户将网络和数据库逻辑联系在一起。
 */ 
 //TransportLayerASIO::_acceptConnection(每个新链接都会创建一个新的session) -> ServiceEntryPointImpl::startSession->ServiceStateMachine::create(每个新链接对应一个ServiceStateMachine结构)
 //ServiceEntryPointImpl::startSession中有新链接的时候构造一个该类  
 //ServiceStateMachine网络收发状态机  
 //一个新链接对应一个ServiceStateMachine保存到ServiceEntryPointImpl._sessions中
class ServiceStateMachine : public std::enable_shared_from_this<ServiceStateMachine> {
    ServiceStateMachine(ServiceStateMachine&) = delete;
    ServiceStateMachine& operator=(ServiceStateMachine&) = delete;

public:
    ServiceStateMachine(ServiceStateMachine&&) = delete;
    ServiceStateMachine& operator=(ServiceStateMachine&&) = delete;

    /*
     * Creates a new ServiceStateMachine for a given session/service context. If sync is true,
     * then calls into the transport layer will block while they complete, otherwise they will
     * be handled asynchronously.
     */
    static std::shared_ptr<ServiceStateMachine> create(ServiceContext* svcContext,
                                                       transport::SessionHandle session,
                                                       transport::Mode transportMode);

    ServiceStateMachine(ServiceContext* svcContext,
                        transport::SessionHandle session,
                        transport::Mode transportMode);

    /*
     * Any state may transition to EndSession in case of an error, otherwise the valid state
     * transitions are:
     * Source -> SourceWait -> Process -> SinkWait -> Source (standard RPC)
     * Source -> SourceWait -> Process -> SinkWait -> Process -> SinkWait ... (exhaust)
     * Source -> SourceWait -> Process -> Source (fire-and-forget)
     */
/*
各状态主要说明如下表所示:
状态    说明    相关函数方法
Created 新链接到来后进入该状态  ServiceStateMachine::create
Source  表示mongodb用户态感知到epoll网络事件通知，开始读取内核态协议栈数据到用户态空间  ServiceStateMachine::_runNextInGuard ServiceStateMachine::_sourceMessage
SourceWait  等待读完内核协议栈数据到用户态，或者epoll_wait超时才返回    ServiceStateMachine::_sourceMessage
Process 读完内核协议栈数据空间到用户态返回，或者epoll_wait超时返回，读取完毕后进行数据解析分析处理  ServiceStateMachine::_sourceCallback ServiceStateMachine::_processMessage
SinkWait    数据发送相关，等待发送数据发送完成或者超时  ServiceStateMachine::_sinkMessage
EndSession  读写异常，或者链接异常的时候进入该状态，表示链接异常需要进行session回收处理 ServiceStateMachine::_runNextInGuard ServiceStateMachine::_sinkCallback ServiceStateMachine::_sourceCallback
Ended   该链接回收完毕，不再可用    ServiceStateMachine::_cleanupSession
*/
    enum class State {
        //ServiceStateMachine::ServiceStateMachine构造函数初始状态
        Created,     // The session has been created, but no operations have been performed yet
        Source,      // Request a new Message from the network to handle
        SourceWait,  // Wait for the new Message to arrive from the network
        Process,     // Run the Message through the database
        SinkWait,    // Wait for the database result to be sent by the network
        EndSession,  // End the session - the ServiceStateMachine will be invalid after this
        Ended        // The session has ended. It is illegal to call any method besides
                     // state() if this is the current state.
    };

    /*
     * When start() is called with Ownership::kOwned, the SSM will swap the Client/thread name
     * whenever it runs a stage of the state machine, and then unswap them out when leaving the SSM.
     *
     * With Ownership::kStatic, it will assume that the SSM will only ever be run from one thread,
     * and that thread will not be used for other SSM's. It will swap in the Client/thread name
     * for the first run and leave them in place.
     *
     * kUnowned is used internally to mark that the SSM is inactive.
     */
    enum class Ownership { 
    kUnowned,  
    //如果是transport::Mode::kSynchronous一个链接一个线程模式，则整个过程中都是同一个线程处理，所以不需要更改线程名
	//如果是async异步线程池模式，则处理链接的过程中会从conn线程变为worker线程
    kOwned, 
    kStatic 
    };

    /*
     * runNext() will run the current state of the state machine. It also handles all the error
     * handling and state management for requests.
     *
     * Each state function (processMessage(), sinkCallback(), etc) should always unwind the stack
     * if they have just completed a database operation to make sure that this doesn't infinitely
     * recurse.
     *
     * runNext() will attempt to create a ThreadGuard when it first runs. If it's unable to take
     * ownership of the SSM, it will call scheduleNext() and return immediately.
     */
    void runNext();

    /*
     * start() schedules a call to runNext() in the future.
     *
     * It is guaranteed to unwind the stack, and not call runNext() recursively, but is not
     * guaranteed that runNext() will run after this return
     */
    void start(Ownership ownershipModel);

    /*
     * Gets the current state of connection for testing/diagnostic purposes.
     */
    State state();

    /*
     * Terminates the associated transport Session, regardless of tags.
     *
     * This will not block on the session terminating cleaning itself up, it returns immediately.
     */
    void terminate();

    /*
     * Terminates the associated transport Session if its tags don't match the supplied tags.
     * If the session is in a pending state, before any tags have been set, it will not be
     * terminated.
     *
     * This will not block on the session terminating cleaning itself up, it returns immediately.
     */
    void terminateIfTagsDontMatch(transport::Session::TagMask tags);

    /*
     * Sets a function to be called after the session is ended
     */
    void setCleanupHook(stdx::function<void()> hook);

private:
    /*
     * A class that wraps up lifetime management of the _dbClient and _threadName for runNext();
     */
    class ThreadGuard;
    friend class ThreadGuard;

    /*
     * Terminates the associated transport Session if status indicate error.
     *
     * This will not block on the session terminating cleaning itself up, it returns immediately.
     */
    void _terminateAndLogIfError(Status status);

    /*
     * This is a helper function to schedule tasks on the serviceExecutor maintaining a shared_ptr
     * copy to anchor the lifetime of the SSM while waiting for callbacks to run.
     *
     * If scheduling the function fails, the SSM will be terminated and cleaned up immediately
     */
    void _scheduleNextWithGuard(ThreadGuard guard,
                                transport::ServiceExecutor::ScheduleFlags flags,
                                Ownership ownershipModel = Ownership::kOwned);

    /*
     * Gets the transport::Session associated with this connection
     */
    const transport::SessionHandle& _session() const;

    /*
     * This is the actual implementation of runNext() that gets called after the ThreadGuard
     * has been successfully created. If any callbacks (like sourceCallback()) need to call
     * runNext() and already own a ThreadGuard, they should call this with that guard as the
     * argument.
     */
    void _runNextInGuard(ThreadGuard guard);

    /*
     * This function actually calls into the database and processes a request. It's broken out
     * into its own inline function for better readability.
     */
    inline void _processMessage(ThreadGuard guard);

    /*
     * These get called by the TransportLayer when requested network I/O has completed.
     */
    void _sourceCallback(Status status);
    void _sinkCallback(Status status);

    /*
     * Source/Sink message from the TransportLayer. These will invalidate the ThreadGuard just
     * before waiting on the TL.
     */
    void _sourceMessage(ThreadGuard guard);
    void _sinkMessage(ThreadGuard guard, Message toSink);

    /*
     * Releases all the resources associated with the session and call the cleanupHook.
     */
    void _cleanupSession(ThreadGuard guard);

    AtomicWord<State> _state{State::Created};

    //ServiceEntryPointMongod 
    ServiceEntryPoint* _sep;
    transport::Mode _transportMode;

    ServiceContext* const _serviceContext;
    
    //TransportLayerASIO::_acceptConnection->ServiceEntryPointImpl::startSession->ServiceStateMachine::create 
    //记录对端信息、同时负责数据相关得读写
    transport::SessionHandle _sessionHandle; //默认对应ASIOSession   
    ServiceContext::UniqueClient _dbClient;
    const Client* _dbClientPtr;
    const std::string _threadName;
    stdx::function<void()> _cleanupHook;

    bool _inExhaust = false;
    boost::optional<MessageCompressorId> _compressorId;
    Message _inMessage; //赋值见ServiceStateMachine::_sourceMessage

    AtomicWord<Ownership> _owned{Ownership::kUnowned};
#if MONGO_CONFIG_DEBUG_BUILD
    AtomicWord<stdx::thread::id> _owningThread;
#endif
    std::string _oldThreadName;
};

template <typename T>
T& operator<<(T& stream, const ServiceStateMachine::State& state) {
    switch (state) {
        case ServiceStateMachine::State::Created:
            stream << "created";
            break;
        case ServiceStateMachine::State::Source:
            stream << "source";
            break;
        case ServiceStateMachine::State::SourceWait:
            stream << "sourceWait";
            break;
        case ServiceStateMachine::State::Process:
            stream << "process";
            break;
        case ServiceStateMachine::State::SinkWait:
            stream << "sinkWait";
            break;
        case ServiceStateMachine::State::EndSession:
            stream << "endSession";
            break;
        case ServiceStateMachine::State::Ended:
            stream << "ended";
            break;
    }
    return stream;
}

}  // namespace mongo
