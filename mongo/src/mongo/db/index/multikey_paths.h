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

#include <cstddef>
#include <set>
#include <vector>

namespace mongo {

// If non-empty, a vector with size equal to the number of elements in the index key pattern. Each
// element in the vector is an ordered set of positions (starting at 0) into the corresponding
// indexed field that represent what prefixes of the indexed field cause the index to be multikey.
//
// For example, with the index {'a.b': 1, 'a.c': 1} where the paths "a" and "a.b" cause the
// index to be multikey, we'd have a std::vector<std::set<size_t>>{{0U, 1U}, {0U}}.
//
// An empty vector is used to represent that the index doesn't support path-level multikey tracking.

/* db.user.ensureIndex({"name":1, "aihao.aa":1, "aihao.bb":1}) db.user.ensureIndex({"name":1, aa:1, bb:1, cc:1})索引对应日志如下：
2021-03-17T18:10:51.944+0800 D STORAGE  [conn-1] recording new metadata: 
{
	md: {
		ns: "test.user",
		options: {
			uuid: UUID("9a09f018-3fb3-4030-b658-680e512c93dd")
		},
		indexes: [{
			spec: {
				v: 2,
				key: {
					_id: 1
				},
				name: "_id_",
				ns: "test.user"
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				_id: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}, {
			spec: {
				v: 2,
				key: {
					name: 1.0,
					aihao.aa: 1.0,
					aihao.bb: 1.0
				},
				name: "name_1_aihao.aa_1_aihao.bb_1",
				ns: "test.user"
			},
			ready: true,
			multikey: true,
			multikeyPaths: {
				name: BinData(0, 00),       //说明是一个独立的字段
				aihao.aa: BinData(0, 0100), //说明是一个a.b类型的索引
				aihao.bb: BinData(0, 0100)  //说明是一个a.b类型的索引
			},
			head: 0,
			prefix: -1
		}, {
			spec: {
				v: 2,
				key: {
					name: 1.0,
					aa: 1.0,
					bb: 1.0,
					cc: 1.0
				},
				name: "name_1_aa_1_bb_1_cc_1",
				ns: "test.user"
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				name: BinData(0, 00),
				aa: BinData(0, 00),
				bb: BinData(0, 00),
				cc: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}],
		prefix: -1
	},
	idxIdent: {
		_id_: "test/index/2--8777216180098127804",
		name_1_aihao.aa_1_aihao.bb_1: "test/index/3--8777216180098127804",
		name_1_aa_1_bb_1_cc_1: "test/index/4--8777216180098127804"
	},
	ns: "test.user",
	ident: "test/collection/1--8777216180098127804"
}
以上打印参考KVCatalog::putMetaData

multikeyPaths: {
    name: BinData(0, 00),       //说明是一个独立的字段
    aihao.aa: BinData(0, 0100), //说明是一个a.b类型的索引
    aihao.bb: BinData(0, 0100)  //说明是一个a.b类型的索引
},

*/


//IndexCatalogEntryImpl._indexMultikeyPaths为该类型
using MultikeyPaths = std::vector<std::set<std::size_t>>;

}  // namespace mongo
