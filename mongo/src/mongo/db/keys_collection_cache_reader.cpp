/**
 *    Copyright (C) 2017 MongoDB, Inc.
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

#include "mongo/db/keys_collection_cache_reader.h"

#include "mongo/db/keys_collection_client.h"
#include "mongo/db/keys_collection_document.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

KeysCollectionCacheReader::KeysCollectionCacheReader(std::string purpose,
                                                     KeysCollectionClient* client)
    : _purpose(std::move(purpose)), _client(client) {}


//KeysCollectionManagerSharding::enableKeyGenerator
//KeysCollectionManagerSharding::startMonitoring定期三个月刷新, startMonitoring
//生成key doc存入keys表和缓存中
StatusWith<KeysCollectionDocument> KeysCollectionCacheReader::refresh(OperationContext* opCtx) {
	
    LogicalTime newerThanThis;

    { //从缓存中能找到缓存中最新的expiresAt时间，则用这个时间，否则用当前时间
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
        auto iter = _cache.crbegin();
        if (iter != _cache.crend()) {
            newerThanThis = iter->second.getExpiresAt();
        }
    }

	//KeysCollectionClientDirect::getNewKeys   
	
	//写数据到"admin.system.keys"中，并返回对应的KeysCollectionDocument
	//查找keys表中小于newerThanThis时间的所有数据并返回
    auto refreshStatus = _client->getNewKeys(opCtx, _purpose, newerThanThis);

    if (!refreshStatus.isOK()) {
        return refreshStatus.getStatus();
    }

    auto& newKeys = refreshStatus.getValue();

    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
	//缓存到内存中
    for (auto&& key : newKeys) {
        _cache.emplace(std::make_pair(key.getExpiresAt(), std::move(key)));
    }

    if (_cache.empty()) {
        return {ErrorCodes::KeyNotFound, "No keys found after refresh"};
    }

    return _cache.crbegin()->second;
}

//KeysCollectionCacheReaderAndUpdater::getKeyById
//KeysCollectionManagerSharding::_getKeyWithKeyIdCheck
StatusWith<KeysCollectionDocument> KeysCollectionCacheReader::getKeyById(
    long long keyId, const LogicalTime& forThisTime) {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);

    for (auto iter = _cache.lower_bound(forThisTime); iter != _cache.cend(); ++iter) {
        if (iter->second.getKeyId() == keyId) {
            return iter->second;
        }
    }

	//内存中找不到该keyID
    return {ErrorCodes::KeyNotFound,
            str::stream() << "Cache Reader No keys found for " << _purpose
                          << " that is valid for time: "
                          << forThisTime.toString()
                          << " with id: "
                          << keyId};
}

//缓存中查找key
StatusWith<KeysCollectionDocument> KeysCollectionCacheReader::getKey(
    const LogicalTime& forThisTime) {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);

    auto iter = _cache.upper_bound(forThisTime);

    if (iter == _cache.cend()) {
        return {ErrorCodes::KeyNotFound,
                str::stream() << "No key found that is valid for " << forThisTime.toString()};
    }

    return iter->second;
}

void KeysCollectionCacheReader::resetCache() {
    // keys that read with non majority readConcern level can be rolled back.
    if (!_client->supportsMajorityReads()) {
        _cache.clear();
    }
}

}  // namespace mongo
