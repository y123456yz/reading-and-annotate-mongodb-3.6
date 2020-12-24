/*
 *    Copyright (C) 2012 10gen Inc.
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

#pragma once

#include "mongo/base/init.h"
#include "mongo/util/fail_point_registry.h"

namespace mongo {

class FailPoint;

/**
 * @return the global fail point registry.
 */
FailPointRegistry* getGlobalFailPointRegistry();

/**
 * Convenience macro for declaring a fail point. Must be used in global scope and never in a
 * block with limited scope (ie, inside functions, loops, etc.).
 *
 * NOTE: Never use in header files, only sources.
 */

//例如MONGO_FP_DECLARE(shutdownAtStartup);   配合Initializer::execute
//定义一个GlobalInitializerRegisterer _mongoInitializerRegisterer_fp类，这些宏定义在main前执行

////MongoDB故障点机制参考https://blog.csdn.net/valada/article/details/104452949
//定义一个故障点 
//添加故障点：Status addFailPoint(const std::string& name, FailPoint* failPoint)
//访问故障点：FailPoint* getFailPoint(const std::string& name) cons


/*
使用故障点进行测试
完成对故障点的定义和故障点触发代码编写后，我们可以在mongod实例启动时通过指定--setParameter enableTestCommands=1作为启动参数来开启configureFailPoint命令。随后我们可以在mongo客户端中通过configureFailPoint命令来配置故障点的模式

启动 mongod,并开启configureFailPoint命令
mongod --setParameter enableTestCommands=1
配置故障点的模式
告诉 mongod 中止接下来的两个网络操作> 
db.adminCommand( { configureFailPoint: "throwSockExcep", mode: {times: 2} } )

最后我们来了解一下故障点的触发模式。MongoDB 为故障点定义了 5 中触发模式：
off(禁用):故障点在程序运行中不触发,即shouldFail()方法总是返回 false。默认模式。
alwaysOn(总是激活):故障点在程序运行中总是可以被触发，即shouldFail()方法总是返回 true.
random(随机激活):故障点在程序运行中的触发次数是随机的。
nTimes(有限次激活):故障点在程序运行中的触发次数是有限的，触发一定次数后，不再触发。
skip(等待有限次再触发):当程序执行到特定故障点的次数超过一定次数后，故障点才触发，并且以后再执行到该故障点时都会触发。
*/
#define MONGO_FP_DECLARE(fp)                                                          \
    FailPoint fp;                                                                     \
    MONGO_INITIALIZER_GENERAL(fp, ("FailPointRegistry"), ("AllFailPointsRegistered")) \
    (::mongo::InitializerContext * context) {                                         \
        return getGlobalFailPointRegistry()->addFailPoint(#fp, &fp);                  \
    } //#fp，为fp字符串，fp为函数FailPoint类指针

/**
 * Convenience macro for defining a fail point in a header scope.
 */
#define MONGO_FP_FORWARD_DECLARE(fp) extern FailPoint fp;

/**
 * Convenience class for enabling a failpoint and disabling it as this goes out of scope.
 */
class FailPointEnableBlock {
public:
    FailPointEnableBlock(const std::string& failPointName);
    ~FailPointEnableBlock();

private:
    FailPoint* _failPoint;
};

}  // namespace mongo
