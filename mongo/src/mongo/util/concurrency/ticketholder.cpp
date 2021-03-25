/*    Copyright 2015 MongoDB Inc.
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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kDefault

#include "mongo/platform/basic.h"

#include "mongo/util/concurrency/ticketholder.h"

#include <iostream>

#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

#if defined(__linux__)
namespace {
void _check(int ret) {
    if (ret == 0)
        return;
    int err = errno;
    severe() << "error in Ticketholder: " << errnoWithDescription(err);
    fassertFailed(28604);
}
}

//信号量生效的地方在//LockerImpl<IsForMMAPV1>::_lockGlobalBegin中调用

//互斥信号量初始化    globalTicketHolder赋值  
//Listener.globalTicketHolder成员为TicketHolder类型  
//TicketHolder openWriteTransaction(128);  TicketHolder openReadTransaction(128);
TicketHolder::TicketHolder(int num) : _outof(num) {
    _check(sem_init(&_sem, 0, num));

	/*
	2019-04-15T17:39:42.799+0800 I -		[main] yang test .........................TicketHolder num:128
	2019-04-15T17:39:42.799+0800 I -		[main] yang test .........................TicketHolder num:128
	2019-04-15T17:39:42.808+0800 I -		[main] yang test .........................TicketHolder num:1000000
	*/
	//log() << "yang test .........................TicketHolder num:" << num;
}

TicketHolder::~TicketHolder() {
    _check(sem_destroy(&_sem));
}

bool TicketHolder::tryAcquire() {
    while (0 != sem_trywait(&_sem)) {
        if (errno == EAGAIN)
            return false;
        if (errno != EINTR)
            _check(-1);
    }
    return true;
}

/*
ticket是引擎可以设置的一个限制。正常情况下，如果没有锁竞争，所有的读写请求都会被pass到引擎层，这样就有个问题，
你请求到了引擎层面，还是得排队执行，而且不同引擎处理能力肯定也不同，于是引擎层就可以通过设置这个ticket，来限
制一下传到引擎层面的最大并发数。比如

wiredtiger设置了读写ticket均为128，也就是说wiredtiger引擎层最多支持128的读写并发（这个值经过测试是非常合理的经验值，无需修改）。
*/
//LockerImpl<IsForMMAPV1>::_lockGlobalBegin中调用
void TicketHolder::waitForTicket() { 
    while (0 != sem_wait(&_sem)) {
        if (errno != EINTR)
            _check(-1);
    }
}

bool TicketHolder::waitForTicketUntil(Date_t until) {
    const long long millisSinceEpoch = until.toMillisSinceEpoch();
    struct timespec ts;

    ts.tv_sec = millisSinceEpoch / 1000;
    ts.tv_nsec = (millisSinceEpoch % 1000) * (1000 * 1000);
    while (0 != sem_timedwait(&_sem, &ts)) {
        if (errno == ETIMEDOUT)
            return false;

        if (errno != EINTR)
            _check(-1);
    }
    return true;
}

void TicketHolder::release() {
    _check(sem_post(&_sem));
}

//checkTicketNumbers中调用，调整大小，最终TicketHolder::_outof = newSize
//TicketServerParameter::_set
Status TicketHolder::resize(int newSize) {
    stdx::lock_guard<stdx::mutex> lk(_resizeMutex);

    if (newSize < 5)
        return Status(ErrorCodes::BadValue,
                      str::stream() << "Minimum value for semaphore is 5; given " << newSize);

    if (newSize > SEM_VALUE_MAX)
        return Status(ErrorCodes::BadValue,
                      str::stream() << "Maximum value for semaphore is " << SEM_VALUE_MAX
                                    << "; given "
                                    << newSize);

    while (_outof.load() < newSize) {
        release();
        _outof.fetchAndAdd(1);
    }

    while (_outof.load() > newSize) {
        waitForTicket();
        _outof.subtractAndFetch(1);
    }

    invariant(_outof.load() == newSize);
    return Status::OK();
}

//还剩多少可用
int TicketHolder::available() const {
    int val = 0;
    _check(sem_getvalue(&_sem, &val));
    return val;
}

//用了多少
int TicketHolder::used() const {
    return outof() - available();
}

////TicketServerParameter::append
//总的信号量
int TicketHolder::outof() const {
    return _outof.load();
}

#else

TicketHolder::TicketHolder(int num) : _outof(num), _num(num) {}

TicketHolder::~TicketHolder() = default;

bool TicketHolder::tryAcquire() {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    return _tryAcquire();
}

void TicketHolder::waitForTicket() {
    stdx::unique_lock<stdx::mutex> lk(_mutex);

    while (!_tryAcquire()) {
        _newTicket.wait(lk);
    }
}

bool TicketHolder::waitForTicketUntil(Date_t until) {
    stdx::unique_lock<stdx::mutex> lk(_mutex);

    return _newTicket.wait_until(lk, until.toSystemTimePoint(), [this] { return _tryAcquire(); });
}

void TicketHolder::release() {
    {
        stdx::lock_guard<stdx::mutex> lk(_mutex);
        _num++;
    }
    _newTicket.notify_one();
}

Status TicketHolder::resize(int newSize) {
    stdx::lock_guard<stdx::mutex> lk(_mutex);

    int used = _outof.load() - _num;
    if (used > newSize) {
        std::stringstream ss;
        ss << "can't resize since we're using (" << used << ") "
           << "more than newSize(" << newSize << ")";

        std::string errmsg = ss.str();
        log() << errmsg;
        return Status(ErrorCodes::BadValue, errmsg);
    }

    _outof.store(newSize);
    _num = _outof.load() - used;

    // Potentially wasteful, but easier to see is correct
    _newTicket.notify_all();
    return Status::OK();
}

int TicketHolder::available() const {
    return _num;
}

int TicketHolder::used() const {
    return outof() - _num;
}

int TicketHolder::outof() const {
    return _outof.load();
}

bool TicketHolder::_tryAcquire() {
    if (_num <= 0) {
        if (_num < 0) {
            std::cerr << "DISASTER! in TicketHolder" << std::endl;
        }
        return false;
    }
    _num--;
    return true;
}
#endif
}
