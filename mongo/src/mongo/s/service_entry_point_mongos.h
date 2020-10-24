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

#include <vector>

#include "mongo/base/disallow_copying.h"
#include "mongo/transport/service_entry_point_impl.h"

namespace mongo {

/**
 * The entry point from the TransportLayer into Mongos.
Tips: 
  mongos和mongod服务入口类为何要继承网络传输模块服务入口类？
原因是一个请求对应一个链接session，该session对应的请求又和SSM状态机唯一对应。所有客户端请求
对应的SSM状态机信息全部保存再ServiceEntryPointImpl._sessions成员中，而command命令处理模块为
SSM状态机任务中的dealTask任务，通过该继承关系，ServiceEntryPointMongod和ServiceEntryPointMongos子
类也就可以和状态机及任务处理关联起来，同时也可以获取当前请求对应的session链接信息。
*/
//runMongosServer中构造使用
//class ServiceEntryPointMongos final : public ServiceEntryPointImpl {
class ServiceEntryPointMongos : public ServiceEntryPointImpl {

    MONGO_DISALLOW_COPYING(ServiceEntryPointMongos);

public:
    using ServiceEntryPointImpl::ServiceEntryPointImpl;
    
    //ServiceEntryPointMongod::handleRequest(mongod网络处理)  ServiceEntryPointMongos::handleRequest mongos网络请求处理
    DbResponse handleRequest(OperationContext* opCtx, const Message& request) override;
};

}  // namespace mongo
