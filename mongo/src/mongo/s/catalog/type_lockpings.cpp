/**
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
#include "mongo/s/catalog/type_lockpings.h"

#include "mongo/base/status_with.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/bson/util/bson_extract.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

/* 记录了和所有节点的ping信息
mongos> db.lockpings.find()
{ "_id" : "ConfigServer", "ping" : ISODate("2020-08-08T15:46:43.471Z") }
{ "_id" : "xxx:20003:1581573543:3630770638362238126", "ping" : ISODate("2020-08-08T15:46:36.839Z") }
//这种说明实例重启了，之前的实例失联了,重启实例后会用新的_id替代该实例，但是之前的实例还是记录在里面的
{ "_id" : "xxx:20003:1581573552:3015220040157753333", "ping" : ISODate("2020-02-13T14:31:15.194Z") } 
{ "_id" : "xxx:20003:1581573564:-3080866622298223419", "ping" : ISODate("2020-08-08T15:46:40.320Z") }
{ "_id" : "xxx:20001:1581573577:-6950517477465643150", "ping" : ISODate("2020-08-08T15:46:33.676Z") }
{ "_id" : "xxx:20001:1581573577:-4720166468454920588", "ping" : ISODate("2020-02-13T08:18:07.712Z") }
{ "_id" : "xxx:20001:1581573577:6146141285149556418", "ping" : ISODate("2020-08-08T15:46:36.501Z") }
{ "_id" : "xxx:20001:1581602007:2653463530376788741", "ping" : ISODate("2020-08-08T15:46:27.902Z") }
{ "_id" : "xxx:20003:1581604307:-5313333738365382099", "ping" : ISODate("2020-08-08T15:46:33.679Z") }
mongos> 

*/

////ReplSetDistLockManager::doTask调用,replSetDistLockPinger线程周期性调用更新该表,30S更新一次
//相关实现见DistLockCatalogImpl
const std::string LockpingsType::ConfigNS = "config.lockpings";

const BSONField<std::string> LockpingsType::process("_id");
const BSONField<Date_t> LockpingsType::ping("ping");

//解析config.lockpings中的数据
StatusWith<LockpingsType> LockpingsType::fromBSON(const BSONObj& source) {
    LockpingsType lpt;

    {
        std::string lptProcess;
        Status status = bsonExtractStringField(source, process.name(), &lptProcess);
        if (!status.isOK())
            return status;
        lpt._process = lptProcess;
    }

    {
        BSONElement lptPingElem;
        Status status = bsonExtractTypedField(source, ping.name(), BSONType::Date, &lptPingElem);
        if (!status.isOK())
            return status;
        lpt._ping = lptPingElem.date();
    }

    return lpt;
}

//lockpings表中的数据有效性检查
Status LockpingsType::validate() const {
    if (!_process.is_initialized() || _process->empty()) {
        return {ErrorCodes::NoSuchKey, str::stream() << "missing " << process.name() << " field"};
    }

    if (!_ping.is_initialized()) {
        return {ErrorCodes::NoSuchKey, str::stream() << "missing " << ping.name() << " field"};
    }

    return Status::OK();
}

//构造config.lockpings数据
BSONObj LockpingsType::toBSON() const {
    BSONObjBuilder builder;

    if (_process)
        builder.append(process.name(), getProcess());
    if (_ping)
        builder.append(ping.name(), getPing());

    return builder.obj();
}

void LockpingsType::setProcess(const std::string& process) {
    invariant(!process.empty());
    _process = process;
}

void LockpingsType::setPing(const Date_t ping) {
    _ping = ping;
}

std::string LockpingsType::toString() const {
    return toBSON().toString();
}

}  // namespace mongo
