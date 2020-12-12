// insert.cpp

/**
 *    Copyright (C) 2008 10gen Inc.
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
#include "mongo/platform/basic.h"

#include "mongo/db/ops/insert.h"

#include <vector>

#include "mongo/bson/bson_depth.h"
#include "mongo/db/logical_clock.h"
#include "mongo/db/logical_time.h"
#include "mongo/db/views/durable_view_catalog.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

using std::string;

using namespace mongoutils;

namespace {
/**
 * Validates the nesting depth of 'obj', returning a non-OK status if it exceeds the limit.
 */ //嵌套文档深度检查  默认嵌套深度180
Status validateDepth(const BSONObj& obj) {
    std::vector<BSONObjIterator> frames;
    frames.reserve(16);
    frames.emplace_back(obj);

    while (!frames.empty()) {
        const auto elem = frames.back().next();
        if (elem.type() == BSONType::Object || elem.type() == BSONType::Array) {
            if (MONGO_unlikely(frames.size() == BSONDepth::getMaxDepthForUserStorage())) {
                // We're exactly at the limit, so descending to the next level would exceed
                // the maximum depth.
                return {ErrorCodes::Overflow,
                        str::stream() << "cannot insert document because it exceeds "
                                      << BSONDepth::getMaxDepthForUserStorage()
                                      << " levels of nesting"};
            }
            frames.emplace_back(elem.embeddedObject());
        }

        if (!frames.back().more()) {
            frames.pop_back();
        }
    }

    return Status::OK();
}
}  // namespace

//对doc文档做检查，返回新的BSONObj  添加ID
//这里面会遍历所有的bson成员elem，并做相应的检查，并给该doc添加相应的ID
StatusWith<BSONObj> fixDocumentForInsert(ServiceContext* service, const BSONObj& doc) {
    if (doc.objsize() > BSONObjMaxUserSize) //一个文档最多16M
        return StatusWith<BSONObj>(ErrorCodes::BadValue,
                                   str::stream() << "object to insert too large"
                                                 << ". size in bytes: "
                                                 << doc.objsize()
                                                 << ", max size: "
                                                 << BSONObjMaxUserSize);


    auto depthStatus = validateDepth(doc);//嵌套文档深度检查，文档嵌套深度不能超过180
    if (!depthStatus.isOK()) {
        return depthStatus;
    }


	//客户端写进来的数据是否带有ID

    bool firstElementIsId = false;
    bool hasTimestampToFix = false;
    bool hadId = false;
    {
		//mongo::BSONOBjIterator 它主要是用来遍历BSONObj对象中的每一个元素，提供了类似于stl iterator的一些接口
        BSONObjIterator i(doc);
        for (bool isFirstElement = true; i.more(); isFirstElement = false) { //文档里面的各个key字段检查
            BSONElement e = i.next();

            if (e.type() == bsonTimestamp && e.timestampValue() == 0) {
                // we replace Timestamp(0,0) at the top level with a correct value
                // in the fast pass, we just mark that we want to swap
                hasTimestampToFix = true;
            }

            auto fieldName = e.fieldNameStringData();

			//文档内容的key第一个字符不能为$
            if (fieldName[0] == '$') {
				/*
				mongos> db.test.insert({"$aa":"bb"})
				WriteResult({
				        "nInserted" : 0,
				        "writeError" : {
				                "code" : 2,
				                "errmsg" : "Document can't have $ prefixed field names: $aa"
				        }
				})
				mongos> 
				*/
                return StatusWith<BSONObj>(
                    ErrorCodes::BadValue,
                    str::stream() << "Document can't have $ prefixed field names: " << fieldName);
            }

            // check no regexp for _id (SERVER-9502)
            // also, disallow undefined and arrays
            // Make sure _id isn't duplicated (SERVER-19361).
            if (fieldName == "_id") { //_id成员
                if (e.type() == RegEx) {
                    return StatusWith<BSONObj>(ErrorCodes::BadValue, "can't use a regex for _id");
                }
                if (e.type() == Undefined) {
                    return StatusWith<BSONObj>(ErrorCodes::BadValue,
                                               "can't use a undefined for _id");
                }
                if (e.type() == Array) {
                    return StatusWith<BSONObj>(ErrorCodes::BadValue, "can't use an array for _id");
                }
                if (e.type() == Object) {
                    BSONObj o = e.Obj();
                    Status s = o.storageValidEmbedded();
                    if (!s.isOK())
                        return StatusWith<BSONObj>(s);
                }
                if (hadId) {
                    return StatusWith<BSONObj>(ErrorCodes::BadValue,
                                               "can't have multiple _id fields in one document");
                } else {
                    hadId = true;
                    firstElementIsId = isFirstElement;
                }
            }
        }
    }

    if (firstElementIsId && !hasTimestampToFix)
        return StatusWith<BSONObj>(BSONObj());

    BSONObjIterator i(doc);

    BSONObjBuilder b(doc.objsize() + 16);
    if (firstElementIsId) { //客户端写进来第一个elem就带有_ID
        b.append(doc.firstElement()); //直接append到新的BSONObjBuilder
        i.next();
    } else { //添加一个_id elem
        BSONElement e = doc["_id"];
        if (e.type()) { //获取_id:XXX中xxx的类型
            b.append(e); 
        } else {  
        	//bson/bsonobjbuilder.h:    BSONObjBuilder& appendOID(StringData fieldName, OID* oid = 0, bool generateIfBlank = false) {
            b.appendOID("_id", NULL, true); 
        }
    }

    while (i.more()) {
        BSONElement e = i.next();
        if (hadId && e.fieldNameStringData() == "_id") {
            // no-op
        } else if (e.type() == bsonTimestamp && e.timestampValue() == 0) { //生成时间elem
            auto nextTime = LogicalClock::get(service)->reserveTicks(1);
            b.append(e.fieldName(), nextTime.asTimestamp());
        } else {
            b.append(e);
        }
    }
    return StatusWith<BSONObj>(b.obj());
}

//检查是否可以对ns进行写操作，有些内部ns是不能写的
Status userAllowedWriteNS(StringData ns) {
    return userAllowedWriteNS(nsToDatabaseSubstring(ns), nsToCollectionSubstring(ns));
}
//检查是否可以对ns进行写操作，有些内部ns是不能写的
Status userAllowedWriteNS(const NamespaceString& ns) {
    return userAllowedWriteNS(ns.db(), ns.coll());
}

Status userAllowedWriteNS(StringData db, StringData coll) {
    if (coll == "system.profile") {
        return Status(ErrorCodes::InvalidNamespace,
                      str::stream() << "cannot write to '" << db << ".system.profile'");
    }
    return userAllowedCreateNS(db, coll);
}

Status userAllowedCreateNS(StringData db, StringData coll) { //库 集合
    // validity checking

    if (db.size() == 0)
        return Status(ErrorCodes::InvalidNamespace, "db cannot be blank");

    if (!NamespaceString::validDBName(db, NamespaceString::DollarInDbNameBehavior::Allow))
        return Status(ErrorCodes::InvalidNamespace, "invalid db name");

    if (coll.size() == 0)
        return Status(ErrorCodes::InvalidNamespace, "collection cannot be blank");

    if (!NamespaceString::validCollectionName(coll))
        return Status(ErrorCodes::InvalidNamespace, "invalid collection name");

    if (db.size() + 1 /* dot */ + coll.size() > NamespaceString::MaxNsCollectionLen)
        return Status(ErrorCodes::InvalidNamespace,
                      str::stream() << "fully qualified namespace " << db << '.' << coll
                                    << " is too long "
                                    << "(max is "
                                    << NamespaceString::MaxNsCollectionLen
                                    << " bytes)");

    // check spceial areas

    if (db == "system")
        return Status(ErrorCodes::InvalidNamespace, "cannot use 'system' database");


    if (coll.startsWith("system.")) {
        if (coll == "system.indexes")
            return Status::OK();
        if (coll == "system.js")
            return Status::OK();
        if (coll == "system.profile")
            return Status::OK();
        if (coll == "system.users")
            return Status::OK();
        if (coll == DurableViewCatalog::viewsCollectionName())
            return Status::OK();
        if (db == "admin") {
            if (coll == "system.version")
                return Status::OK();
            if (coll == "system.roles")
                return Status::OK();
            if (coll == "system.new_users")
                return Status::OK();
            if (coll == "system.backup_users")
                return Status::OK();
            if (coll == "system.keys")
                return Status::OK();
        }
        if (db == "config") {
            if (coll == "system.sessions")
                return Status::OK();
        }
        if (db == "local") {
            if (coll == "system.replset")
                return Status::OK();
            if (coll == "system.healthlog")
                return Status::OK();
        }
        return Status(ErrorCodes::InvalidNamespace,
                      str::stream() << "cannot write to '" << db << "." << coll << "'");
    }

    // some special rules

    if (coll.find(".system.") != string::npos) {
        // If this is metadata for the sessions collection, shard servers need to be able to
        // write to it.
        if (coll.find(".system.sessions") != string::npos) {
            return Status::OK();
        }

        // this matches old (2.4 and older) behavior, but I'm not sure its a good idea
        return Status(ErrorCodes::BadValue,
                      str::stream() << "cannot write to '" << db << "." << coll << "'");
    }

    return Status::OK();
}
}
