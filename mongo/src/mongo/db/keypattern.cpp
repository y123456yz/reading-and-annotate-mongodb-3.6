// @file keypattern.cpp

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
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#include "mongo/db/keypattern.h"

#include "mongo/db/index_names.h"

namespace mongo {

KeyPattern::KeyPattern(const BSONObj& pattern) : _pattern(pattern) {}

//判断是否id范围索引
bool KeyPattern::isIdKeyPattern(const BSONObj& pattern) {
    BSONObjIterator i(pattern);
    BSONElement e = i.next();
    // _id index must have form exactly {_id : 1} or {_id : -1}.
    // Allows an index of form {_id : "hashed"} to exist but
    // do not consider it to be the primary _id index
    return (0 == strcmp(e.fieldName(), "_id")) && (e.numberInt() == 1 || e.numberInt() == -1) &&
        i.next().eoo();
}

//是否为普通索引，如"xx":1
bool KeyPattern::isOrderedKeyPattern(const BSONObj& pattern) {
    return IndexNames::BTREE == IndexNames::findPluginName(pattern);
}

//是否为hash索引
bool KeyPattern::isHashedKeyPattern(const BSONObj& pattern) {
    return IndexNames::HASHED == IndexNames::findPluginName(pattern);
}

//获取所有字符串 例如： {"AA":1, "BB":-1}   {"AA":"hashed"}
StringBuilder& operator<<(StringBuilder& sb, const KeyPattern& keyPattern) {
    // Rather than return BSONObj::toString() we construct a keyPattern string manually. This allows
    // us to avoid the cost of writing numeric direction to the str::stream which will then undergo
    // expensive number to string conversion.
    sb << "{ ";

    bool first = true;
    for (auto&& elem : keyPattern._pattern) {
        if (first) {
            first = false;
        } else {
            sb << ", ";
        }

        if (mongo::String == elem.type()) { //说明是字符串，例如hash  2d text索引都是字符串
            sb << elem;
        } else if (elem.number() >= 0) {
            // The canonical check as to whether a key pattern element is "ascending" or
            // "descending" is (elem.number() >= 0). This is defined by the Ordering class.
            sb << elem.fieldNameStringData() << ": 1";
        } else {
            sb << elem.fieldNameStringData() << ": -1";
        }
    }

    sb << " }";
    return sb;
}

/* Takes a BSONObj whose field names are a prefix of the fields in this keyPattern, and
 * outputs a new bound with MinKey values appended to match the fields in this keyPattern
 * (or MaxKey values for descending -1 fields). This is useful in sharding for
 * calculating chunk boundaries when tag ranges are specified on a prefix of the actual
 * shard key, or for calculating index bounds when the shard key is a prefix of the actual
 * index used.
 *
 * @param makeUpperInclusive If true, then MaxKeys instead of MinKeys will be appended, so
 * that the output bound will compare *greater* than the bound being extended (note that
 * -1's in the keyPattern will swap MinKey/MaxKey vals. See examples).
 *
 * Examples:
 * If this keyPattern is {a : 1}
 *	 extendRangeBound( {a : 55}, false) --> {a : 55}
 *
 * If this keyPattern is {a : 1, b : 1}
 *	 extendRangeBound( {a : 55}, false) --> {a : 55, b : MinKey}
 *	 extendRangeBound( {a : 55}, true ) --> {a : 55, b : MaxKey}
 *
 * If this keyPattern is {a : 1, b : -1}
 *	 extendRangeBound( {a : 55}, false) --> {a : 55, b : MaxKey}
 *	 extendRangeBound( {a : 55}, true ) --> {a : 55, b : MinKey}
 */
//bound做转换，如上备注
BSONObj KeyPattern::extendRangeBound(const BSONObj& bound, bool makeUpperInclusive) const {
    BSONObjBuilder newBound(bound.objsize());

    BSONObjIterator src(bound);
    BSONObjIterator pat(_pattern);

    while (src.more()) {
        massert(16649,
                str::stream() << "keyPattern " << _pattern << " shorter than bound " << bound,
                pat.more());
        BSONElement srcElt = src.next();
        BSONElement patElt = pat.next();
        massert(16634,
                str::stream() << "field names of bound " << bound
                              << " do not match those of keyPattern "
                              << _pattern,
                str::equals(srcElt.fieldName(), patElt.fieldName()));
        newBound.append(srcElt);
    }
    while (pat.more()) {
        BSONElement patElt = pat.next();
        // for non 1/-1 field values, like {a : "hashed"}, treat order as ascending
        int order = patElt.isNumber() ? patElt.numberInt() : 1;
        // flip the order semantics if this is an upper bound
        if (makeUpperInclusive)
            order *= -1;

        if (order > 0) {
            newBound.appendMinKey(patElt.fieldName());
        } else {
            newBound.appendMaxKey(patElt.fieldName());
        }
    }
    return newBound.obj();
}

BSONObj KeyPattern::globalMin() const {
    return extendRangeBound(BSONObj(), false);
}

BSONObj KeyPattern::globalMax() const {
    return extendRangeBound(BSONObj(), true);
}

}  // namespace mongo
