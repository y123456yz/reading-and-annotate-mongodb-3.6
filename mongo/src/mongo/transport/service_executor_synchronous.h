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

#include <deque>

#include "mongo/base/status.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/condition_variable.h"
#include "mongo/stdx/mutex.h"
#include "mongo/transport/service_executor.h"

namespace mongo {
namespace transport {

/**
 * The passthrough service executor emulates a thread per connection.
 * Each connection has its own worker thread where jobs get scheduled.
 * createWithConfig中构造使用
 */
/*
"adaptive") : <ServiceExecutorAdaptive>( 引入了boost.asio库实现网络接口的异步调用并作为默认配置，同时还把线程模型调整
为线程池，动态根据workload压力情况调整线程数量，在大量连接情况下可以避免产生大量的处理线程，降低线程切换开销，以获得
更稳定的性能表现。

"synchronous"): <ServiceExecutorSynchronous>(ctx));  每个网络连接创建一个专用线程，并同步调用网络接口recv/send收发包
}
*/

//构建使用见TransportLayerManager::createWithConfig,最终赋值给ServiceContext._serviceExecutor
//ServiceExecutorSynchronous对应线程池同步模式，ServiceExecutorAdaptive对应线程池异步自适应模式，他们的作用是处理链接相关的线程模型
class ServiceExecutorSynchronous final : public ServiceExecutor {
public:
    explicit ServiceExecutorSynchronous(ServiceContext* ctx);

    Status start() override;
    Status shutdown(Milliseconds timeout) override;
    Status schedule(Task task, ScheduleFlags flags) override;

    Mode transportMode() const override {
        return Mode::kSynchronous;
    }

    void appendStats(BSONObjBuilder* bob) const override;

private:
/* thread_local变量初始化
thread_local std::deque<ServiceExecutor::Task> ServiceExecutorSynchronous::_localWorkQueue = {}; //链接入队
thread_local int ServiceExecutorSynchronous::_localRecursionDepth = 0;
thread_local int64_t ServiceExecutorSynchronous::_localThreadIdleCounter = 0;
*/
    static thread_local std::deque<Task> _localWorkQueue;
    static thread_local int _localRecursionDepth;
    static thread_local int64_t _localThreadIdleCounter;

    AtomicBool _stillRunning{false};

    mutable stdx::mutex _shutdownMutex;
    stdx::condition_variable _shutdownCondition;

    //当前conn线程数，参考ServiceExecutorSynchronous::schedul 
    //注意，这是个全局的，表示有多少个线程，前面的_localWorkQueue等是线程级别的
    AtomicWord<size_t> _numRunningWorkerThreads{0};
    size_t _numHardwareCores{0}; //cpu个数
};

}  // namespace transport
}  // namespace mongo
