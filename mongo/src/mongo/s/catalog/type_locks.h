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

#pragma once

#include <boost/optional.hpp>
#include <string>

#include "mongo/db/jsobj.h"
#include "mongo/util/time_support.h"

namespace mongo {

/**
 * This class represents the layout and contents of documents contained in the
 * config.locks collection. All manipulation of documents coming from that
 * collection should be done with this class.
 */
/*
mongos> use test3
switched to db test3
mongos>  db.test3.insert({ "_id" : "test2-movePrimary", "state" : 0, "process" : "ConfigServer", "ts" : ObjectId("5f2a8d3abd120dda8009f247"), "when" : ISODate("2020-08-05T10:43:06.205Z"), "who" : "ConfigServer:conn731", "why" : "enableSharding" })
WriteResult({ "nInserted" : 1 })
mongos> 
mongos> 
mongos>  db.test3.findAndModify ( {query: { _id: "test2-movePrimary", state: 0 },update: { $set: { ts: ObjectId('5f2a8d3abd120dda8009f247'), state: 2, who: "ConfigServer:conn731", process: "ConfigServer", when: new Date(1596624186205), why: "enableSharding" } }, upsert: true, new: true})
{
        "_id" : "test2-movePrimary",
        "state" : 2,
        "process" : "ConfigServer",
        "ts" : ObjectId("5f2a8d3abd120dda8009f247"),
        "when" : ISODate("2020-08-05T10:43:06.205Z"),
        "who" : "ConfigServer:conn731",
        "why" : "enableSharding"
}
mongos> 
mongos> 
mongos> db.test3.findAndModify ( {query: { _id: "test2-movePrimary", state: 0 },update: { $set: { ts: ObjectId('5f2a8d3abd120dda8009f247'), state: 2, who: "ConfigServer:conn731", process: "ConfigServer", when: new Date(1596624186205), why: "enableSharding" } }, upsert: true, new: true})
2020-08-05T18:58:00.251+0800 E QUERY    [thread1] Error: findAndModifyFailed failed: {
        "ok" : 0,
        "errmsg" : "E11000 duplicate key error collection: test3.test3 index: _id_ dup key: { : \"test2-movePrimary\" }",
        "code" : 11000,
        "codeName" : "DuplicateKey",
        "operationTime" : Timestamp(1596625067, 2),
        "$clusterTime" : {
                "clusterTime" : Timestamp(1596625074, 1),
                "signature" : {
                        "hash" : BinData(0,"rv/1oEHfIuhjI2QFhzv6/Gnx2FQ="),
                        "keyId" : NumberLong("6854353039424225306")
                }
        }
} :
_getErrorWithCode@src/mongo/shell/utils.js:25:13
DBCollection.prototype.findAndModify@src/mongo/shell/collection.js:724:1
@(shell):1:1
mongos> 
*/

//findAndModify获取分布式锁返回结果 DistLockCatalogImpl::grabLock
class LocksType {
public:
    //{ "_id" : "push_stat", "state" : 0, "process" : "ConfigServer", "ts" : ObjectId("5e4501a8de959d2ee8e062c2"), "when" : ISODate("2020-02-13T07:58:32.796Z"), "who" : "ConfigServer:conn57", "why" : "createDatabase" }
    enum State {
        //locks表中state字段值为0，表示可以获取到锁
        UNLOCKED = 0,
        LOCK_PREP,  // Only for legacy 3 config servers.
        //
        LOCKED,
        numStates
    };

    // Name of the locks collection in the config server.
    static const std::string ConfigNS;
    
    // Field names and types in the locks collection type.
    /*
    config.locks集合中的数据成员名，也就是KEY
    const BSONField<std::string> LocksType::name("_id");
    const BSONField<LocksType::State> LocksType::state("state");
    const BSONField<std::string> LocksType::process("process");
    const BSONField<OID> LocksType::lockID("ts");
    const BSONField<std::string> LocksType::who("who");
    const BSONField<std::string> LocksType::why("why");
    const BSONField<Date_t> LocksType::when("when");
    */
    static const BSONField<std::string> name;
    static const BSONField<State> state;
    static const BSONField<std::string> process;
    static const BSONField<OID> lockID;
    static const BSONField<std::string> who;
    static const BSONField<std::string> why;
    static const BSONField<Date_t> when;

    /**
     * Constructs a new LocksType object from BSON.
     * Also does validation of the contents.
     */
    static StatusWith<LocksType> fromBSON(const BSONObj& source);

    /**
     * Returns OK if all fields have been set. Otherwise, returns NoSuchKey
     * and information about the first field that is missing.
     */
    Status validate() const;

    /**
     * Returns the BSON representation of the entry.
     */
    BSONObj toBSON() const;

    /**
     * Returns a std::string representation of the current internal state.
     */
    std::string toString() const;

    const std::string& getName() const {
        return _name.get();
    }
    void setName(const std::string& name);

    State getState() const {
        return _state.get();
    }
    void setState(const State state);

    const std::string& getProcess() const {
        return _process.get();
    }
    void setProcess(const std::string& process);

    const OID& getLockID() const {
        return _lockID.get();
    }
    bool isLockIDSet() const {
        return _lockID.is_initialized() && _lockID->isSet();
    }
    void setLockID(const OID& lockID);

    const std::string& getWho() const {
        return _who.get();
    }
    void setWho(const std::string& who);

    const std::string& getWhy() const {
        return _why.get();
    }
    void setWhy(const std::string& why);

private:
    // Convention: (M)andatory, (O)ptional, (S)pecial rule.

    //findAndModify的执行结果存入下列变量
    // (M) name of the lock  也就是locks表中的_id
    boost::optional<std::string> _name;
    // (M) State of the lock (see LocksType::State)
    //也就是config.locks表中的"state"字段
    boost::optional<State> _state;
    // (O) optional if unlocked. Contains the (unique) identifier.
    //也就是config.locks表中的"process"字段
    boost::optional<std::string> _process;
    // (O) optional if unlocked. A unique identifier for the instance.
    //也就是config.locks表中的"ts"字段
    boost::optional<OID> _lockID;
    // (O) optional if unlocked. A note about why the lock is held.
    //也就是config.locks表中的"who"字段
    boost::optional<std::string> _who;
    // (O) optional if unlocked. A human readable description of why the lock is held.
    //也就是config.locks表中的"why"字段
    boost::optional<std::string> _why;
};

}  // namespace mongo
