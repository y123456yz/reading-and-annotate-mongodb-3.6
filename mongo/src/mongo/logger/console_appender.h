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

#pragma once


#include "mongo/base/disallow_copying.h"
#include "mongo/base/status.h"
#include "mongo/logger/appender.h"
#include "mongo/logger/console.h"
#include "mongo/logger/encoder.h"

namespace mongo {
namespace logger {

/**
 * Appender for writing to the console (stdout).
 */
/*
Appender
和其他日志系统一样，Appender就是日志输出到什么地方，常见的有Console、File、syslog等。作为一个服务，日志切割是普遍需求，为此MongoDB实现了一个RotatableFileAppender结合RotatableFileWriter和RotatableFileManager来实现这个功能。
每个Appender类有一个append()接口，所有派生类都需要实现这个接口。Appender类在构造时需要指定一个Encoder，append()的时候调用Encoder的encode()接口对日志进行格式化，然后再输出。MongoDB目前定义了以下几种Appender分别用于console、File和syslog输出：

ConsoleAppender
RotatableFileAppender
SyslogAppender

*/
template <typename Event, typename ConsoleType = Console>
class ConsoleAppender : public Appender<Event> {
    MONGO_DISALLOW_COPYING(ConsoleAppender);

public:
    typedef Encoder<Event> EventEncoder;

    explicit ConsoleAppender(EventEncoder* encoder) : _encoder(encoder) {}
    virtual Status append(const Event& event) {
        ConsoleType console;
        _encoder->encode(event, console.out()).flush();
        if (!console.out())
            return Status(ErrorCodes::LogWriteFailed, "Error writing log message to console.");
        return Status::OK();
    }

private:
    std::unique_ptr<EventEncoder> _encoder;
};

}  // namespace logger
}  // namespace mongo
