/*    Copyright 2013 10gen Inc.
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

#include "mongo/logger/logger.h"

#include "mongo/base/init.h"
#include "mongo/base/status.h"
#include "mongo/platform/compiler.h"

namespace mongo {
/*
MongoDB启动时在GlobalLogManager这个全局初始化函数中实例化了全局的LogManager单例，其中包含了ComponentMessageLogDomain这个全局的LogDomain。因此我们只只需要在需要记log的地方构造LogstreamBuilder，再传入日志内容就可以了。

在util/log.h中定义了以下函数直接构造一个对应的LogstreamBuilder（使用全局的LogDomain和MONGO_LOG_DEFAULT_COMPONENT）：

severe()
error()
warning()
log()
此外，还定义了以下一系列LOG宏来根据传入的DLEVEL值是否大于组件配置的日志级别的值来判断是否需要记录log：

LOG
MONGO_LOG(DLEVEL)
MONGO_LOG_COMPONENT(DLEVEL, COMPONENT1)
MONGO_LOG_COMPONENT2(DLEVEL, COMPONENT1, COMPONENT2)
MONGO_LOG_COMPONENT3(DLEVEL, COMPONENT1, COMPONENT2, COMPONENT3)
这种方式可以指定DLEVEL，因此更加灵活。

综上，MongoDB日志系统的使用非常简单，只需：

在cpp文件中include util/log.h头文件，注意不可include多次
在当前cpp文件中定义一个MONGO_LOG_DEFAULT_COMPONENT宏
使用预定义的几个函数或LOG系列宏来进行log调用
参考https://yq.aliyun.com/articles/5528
*/
namespace logger {

static LogManager* theGlobalLogManager;  // NULL at program start, before even static
                                         // initialization.

//日志切割相关
static RotatableFileManager theGlobalRotatableFileManager;

LogManager* globalLogManager() {
    if (MONGO_unlikely(!theGlobalLogManager)) {
        theGlobalLogManager = new LogManager;
    }
    return theGlobalLogManager;
}

RotatableFileManager* globalRotatableFileManager() {
    return &theGlobalRotatableFileManager;
}

/**
 * Just in case no static initializer called globalLogManager, make sure that the global log
 * manager is instantiated while we're still in a single-threaded context.
 */
MONGO_INITIALIZER_GENERAL(GlobalLogManager, ("ValidateLocale"), ("default"))(InitializerContext*) {
    globalLogManager();
    return Status::OK();
}

}  // namespace logger
}  // namespace mongo
