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

#include <string>

#include "mongo/base/string_data.h"
#include "mongo/bson/oid.h"
#include "mongo/util/time_support.h"

namespace mongo {

/**
 * Data structure for storing information about distributed lock pings.
 */ 
 //ReplSetDistLockManager._pingHistory字段，
 //类成员赋值见ReplSetDistLockManager::isLockExpired
struct DistLockPingInfo {
    DistLockPingInfo();
    DistLockPingInfo(StringData processId,
                     Date_t lastPing,
                     Date_t configLocalTime,
                     OID lockSessionId,
                     OID electionId);


    // the process processId of the last known owner of the lock.
    //也就是config.locks中的process字段，即config.lockpings中的_id字段
    //每个mongo实例有一个唯一的electionId，发生主从切换则会自增，例如从"electionId" : ObjectId("7fffffff0000000000000006"),到"electionId" : ObjectId("7fffffff0000000000000007"),
    std::string processId;

    // the ping value from the last owner of the lock.
    //config.lockpings中的ping字段内容
    Date_t lastPing;

    // the config server local time when this object was updated.
    //也就是db.serverStatus().localTime
    Date_t configLocalTime;

    // last known owner of the lock.
    //config.locks表中的ts字段
    OID lockSessionId;

    // the election id of the config server when this object was updated.
    // Note: unused by legacy dist lock.
    //db.serverStatus().repl.electionId获取的值
    OID electionId; //每隔mongo实例对应一个唯一的electionId
};
}
