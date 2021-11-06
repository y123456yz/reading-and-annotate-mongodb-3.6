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

#include "mongo/base/status.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/platform/bitwise_enum_operators.h"
#include "mongo/stdx/functional.h"
#include "mongo/transport/transport_mode.h"
#include "mongo/util/duration.h"

namespace mongo {
// This needs to be forward declared here because the service_context.h is a circular dependency.
class ServiceContext;

namespace transport {

/*
 * This is the interface for all ServiceExecutors.
 */
//ServiceExecutorAdaptive(动态线程池模式)和ServiceExecutorSynchronous(同步线程,一个链接一个线程)继承该类
//ServiceContext:_serviceExecutor成员为该类类型，setServiceExecutor中让它和ServiceContext关联在一起，线程池相关的类
class ServiceExecutor {
public:
    virtual ~ServiceExecutor() = default;
    using Task = stdx::function<void()>;
    enum ScheduleFlags {
        // No flags (kEmptyFlags) specifies that this is a normal task and that the executor should
        // launch new threads as needed to run the task.
        kEmptyFlags = 1 << 0,

        // Deferred tasks will never get a new thread launched to run them.
        //延迟任务表示该任务不会触发控制线程去启动新的线程，针对adaptive线程模型有效
        //生效见ServiceExecutorAdaptive::schedule
        kDeferredTask = 1 << 1, //State::Source阶段拥有该标识

        // MayRecurse indicates that a task may be run recursively.

        /*
            递归执行过程(以递归深度3为例)：
            task1_pop_run {
                
                task1_run()
                //调用task2执行
                task2_pop_run {
                    task2_run() 
                    task3_pop_run {
                        task3_run() 
                        //task3 end
                        --recursionDepth
                    }
                    
                    //task2 end
                    --recursionDepth
                }
                
                //task1 end
                --recursionDepth
            }
         */

        
        //ServiceStateMachine::_sourceCallback中赋值调用
        //真正生效见ServiceExecutorAdaptive::schedule
        //表示本线程可以继续递归进行多个链接数据的_processMessage处理，一个线程同时处理最大adaptiveServiceExecutorRecursionLimit个链接的_processMessage处理

        //_processMessage递归调用背景: (读取一个完整报文+后续处理，第二个任务递归调用) 多线程环境一个线程可以处理多个链接的请求，因为发送数据给客户端可能是异步的，所以存在同时处理多个链接请求的情况
        //实际上一个线程从boost-asio库的全局队列获取任务执行，当链接数据通过_processMessage
        //转发到后端进入SinkWait状态的时候，_sinkMessage可能会立马成功，就会进入到_sinkCallback
        //进入到State::Source状态，在下一个调度中就会继续_sourceMessage->_sourceCallback进行递归调用
        kMayRecurse = 1 << 2,  

        // MayYieldBeforeSchedule indicates that the executor may yield on the current thread before
        // scheduling the task.
        //针对sync线程模式有效,处理完一个完整请求并返回给客户端后，进行下一次请求处理的时候
        //ServiceStateMachine::_sinkCallback赋值使用，真正生效见ServiceExecutorSynchronous::schedule
        kMayYieldBeforeSchedule = 1 << 3, //等待一个调度的时间，下次执行
    };

    /*
     * Starts the ServiceExecutor. This may create threads even if no tasks are scheduled.
     */
    virtual Status start() = 0;

    /*
     * Schedules a task with the ServiceExecutor and returns immediately.
     *
     * This is guaranteed to unwind the stack before running the task, although the task may be
     * run later in the same thread.
     *
     * If defer is true, then the executor may defer execution of this Task until an available
     * thread is available.
     */
    virtual Status schedule(Task task, ScheduleFlags flags) = 0;

    /*
     * Stops and joins the ServiceExecutor. Any outstanding tasks will not be executed, and any
     * associated callbacks waiting on I/O may get called with an error code.
     *
     * This should only be called during server shutdown to gracefully destroy the ServiceExecutor
     */
    virtual Status shutdown(Milliseconds timeout) = 0;

    /*
     * Returns if this service executor is using asynchronous or synchronous networking.
     */
    virtual Mode transportMode() const = 0;

    /*
     * Appends statistics about task scheduling to a BSONObjBuilder for serverStatus output.
     */
    virtual void appendStats(BSONObjBuilder* bob) const = 0;
};

}  // namespace transport

ENABLE_BITMASK_OPERATORS(transport::ServiceExecutor::ScheduleFlags)

}  // namespace mongo
