/**
 *    Copyright (C) 2015 MongoDB Inc.
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

#include <mutex>

namespace mongo {
namespace stdx {

using ::std::mutex;            // NOLINT
/*
std::recursive_mutex 介绍:
std::recursive_mutex 与 std::mutex 一样，也是一种可以被上锁的对象，但是和 std::mutex 不同的是，std::recursive_mutex 
允许同一个线程对互斥量多次上锁（即递归上锁），来获得对互斥量对象的多层所有权，std::recursive_mutex 释放互斥量时需要
调用与该锁层次深度相同次数的 unlock()，可理解为 lock() 次数和 unlock() 次数相同

std::time_mutex 介绍:
try_lock_for 函数接受一个时间范围，表示在这一段时间范围之内线程如果没有获得锁则被阻塞住（与 std::mutex 的 try_lock() 不同，
try_lock 如果被调用时没有获得锁则直接返回 false），如果在此期间其他线程释放了锁，则该线程可以获得对互斥量的锁，如果超时（
即在指定时间内还是没有获得锁），则返回 false。
*/
using ::std::timed_mutex;      // NOLINT
using ::std::recursive_mutex;  // NOLINT

/*
互斥对象管理类模板的加锁策略
前面提到std::lock_guard、std::unique_lock和std::shared_lock类模板在构造时是否加锁是可选的，C++11提供了3种加锁策略。

策略	tag type	描述
(默认)	无	请求锁，阻塞当前线程直到成功获得锁。
std::defer_lock	std::defer_lock_t	不请求锁。
std::try_to_lock	std::try_to_lock_t	尝试请求锁，但不阻塞线程，锁不可用时也会立即返回。
std::adopt_lock	std::adopt_lock_t	假定当前线程已经获得互斥对象的所有权，所以不再请求锁。
*/
using ::std::adopt_lock_t;   // NOLINT
using ::std::defer_lock_t;   // NOLINT
using ::std::try_to_lock_t;  // NOLINT

using ::std::lock_guard;   // NOLINT
using ::std::unique_lock;  // NOLINT

/*
constexpr:
常量表达式主要是允许一些计算发生在编译时，即发生在代码编译而不是运行的时候。这是很大的优化：假如有些事情可以在编译时做，
它将只做一次，而不是每次程序运行时。需要计算一个编译时已知的常量，比如特定值的sine或cosin？确实你亦可以使用库函数sin或cos，
但那样你必须花费运行时的开销。使用constexpr，你可以创建一个编译时的函数，它将为你计算出你需要的数值。

一个constexpr有一些必须遵循的严格要求：
    函数中只能有一个return语句（有极少特例）
    只能调用其它constexpr函数
    只能使用全局constexpr变量
*/
constexpr adopt_lock_t adopt_lock{};
constexpr defer_lock_t defer_lock{};
constexpr try_to_lock_t try_to_lock{};

}  // namespace stdx
}  // namespace mongo

