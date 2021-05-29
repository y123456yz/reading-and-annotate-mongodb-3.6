/**
*    Copyright (C) 2013 10gen Inc.
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

#include "mongo/db/index/btree_access_method.h"

#include <vector>

#include "mongo/base/status.h"
#include "mongo/base/status_with.h"
#include "mongo/db/catalog/index_catalog_entry.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/keypattern.h"

namespace mongo {

using std::vector;

// Standard Btree implementation below.
//KVDatabaseCatalogEntry::getIndex中构造初始化
BtreeAccessMethod::BtreeAccessMethod(IndexCatalogEntry* btreeState, SortedDataInterface* btree)
    : IndexAccessMethod(btreeState, btree) {
    // The key generation wants these values.
    vector<const char*> fieldNames;
    vector<BSONElement> fixed;


	/** keyPattern内容如下
     * Return the user-provided index key pattern.
     * Example: {geo: "2dsphere", nonGeo: 1}
     * Example: {foo: 1, bar: -1}
     */
    BSONObjIterator it(_descriptor->keyPattern());
    while (it.more()) {
        BSONElement elt = it.next();
        fieldNames.push_back(elt.fieldName());
        fixed.push_back(BSONElement());
    }

	//一个索引对应一个BtreeKeyGenerator
    _keyGenerator = BtreeKeyGenerator::make(_descriptor->version(),
                                            fieldNames,
                                            fixed,
                                            _descriptor->isSparse(),
                                            btreeState->getCollator());
    massert(16745, "Invalid index version for key generation.", _keyGenerator);
}

//例如{aa:1, bb:1}索引，doc数据:{aa:xx1, bb:xx2}，则keys为xx1_xx2
    
//如果是数组索引，例如{a.b : 1, c:1}，数据为{c:xxc, a:[{b:xxb1},{b:xxb2}]},
//则keys会生成两条数据[xxb1_xxc、xxb2_xxc]

//IndexAccessMethod::getKeys->IndexAccessMethod::getKeys
void BtreeAccessMethod::doGetKeys(const BSONObj& obj,  //数据value
                                  BSONObjSet* keys,
                                  MultikeyPaths* multikeyPaths) const {
    
    
	//IndexAccessMethod::getKeys->IndexAccessMethod::getKeys->BtreeKeyGenerator::getKeys->BtreeKeyGeneratorV1::getKeysImpl

	//BtreeKeyGenerator::getKeys
	_keyGenerator->getKeys(obj, keys, multikeyPaths);
}

}  // namespace mongo
