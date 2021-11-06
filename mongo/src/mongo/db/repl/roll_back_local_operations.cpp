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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kReplicationRollback

#include "mongo/platform/basic.h"

#include "mongo/db/repl/roll_back_local_operations.h"

#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {
namespace repl {

// After the release of MongoDB 3.8, these fail point declarations can
// be moved into the rs_rollback.cpp file, as we no longer need to maintain
// functionality for rs_rollback_no_uuid.cpp. See SERVER-29766.

// Failpoint which causes rollback to hang before finishing.
MONGO_FP_DECLARE(rollbackHangBeforeFinish);

// Failpoint which causes rollback to hang and then fail after minValid is written.
MONGO_FP_DECLARE(rollbackHangThenFailAfterWritingMinValid);


namespace {

OpTime getOpTime(const OplogInterface::Iterator::Value& oplogValue) {
    return fassertStatusOK(40298, OpTime::parseFromOplogEntry(oplogValue.first));
}

Timestamp getTimestamp(const BSONObj& operation) {
    return operation["ts"].timestamp();
}

Timestamp getTimestamp(const OplogInterface::Iterator::Value& oplogValue) {
    return getTimestamp(oplogValue.first);
}

long long getHash(const BSONObj& operation) {
    return operation["h"].Long();
}

long long getHash(const OplogInterface::Iterator::Value& oplogValue) {
    return getHash(oplogValue.first);
}

}  // namespace

RollBackLocalOperations::RollBackLocalOperations(const OplogInterface& localOplog,
                                                 const RollbackOperationFn& rollbackOperation)

    : _localOplogIterator(localOplog.makeIterator()),
      _rollbackOperation(rollbackOperation),
      _scanned(0) {
    uassert(ErrorCodes::BadValue, "invalid local oplog iterator", _localOplogIterator);
    uassert(ErrorCodes::BadValue, "null roll back operation function", rollbackOperation);
}

StatusWith<RollBackLocalOperations::RollbackCommonPoint> RollBackLocalOperations::onRemoteOperation(
    const BSONObj& operation) {
    if (_scanned == 0) {
        auto result = _localOplogIterator->next();
        if (!result.isOK()) {
            return StatusWith<RollbackCommonPoint>(ErrorCodes::OplogStartMissing,
                                                   "no oplog during initsync");
        }
        _localOplogValue = result.getValue();

        long long diff = static_cast<long long>(getTimestamp(_localOplogValue).getSecs()) -
            getTimestamp(operation).getSecs();
        // diff could be positive, negative, or zero
        log() << "our last optime:   " << getTimestamp(_localOplogValue);
        log() << "their last optime: " << getTimestamp(operation);
        log() << "diff in end of log times: " << diff << " seconds";
        if (diff > 1800) {
            severe() << "rollback too long a time period for a rollback.";
            return StatusWith<RollbackCommonPoint>(
                ErrorCodes::ExceededTimeLimit,
                "rollback error: not willing to roll back more than 30 minutes of data");
        }
    }

    while (getTimestamp(_localOplogValue) > getTimestamp(operation)) {
        _scanned++;
        LOG(2) << "Local oplog entry to roll back: " << redact(_localOplogValue.first);
        auto status = _rollbackOperation(_localOplogValue.first);
        if (!status.isOK()) {
            invariant(ErrorCodes::NoSuchKey != status.code());
            return status;
        }
        auto result = _localOplogIterator->next();
        if (!result.isOK()) {
            severe() << "rollback error RS101 reached beginning of local oplog";
            log() << "    scanned: " << _scanned;
            log() << "  theirTime: " << getTimestamp(operation);
            log() << "  ourTime:   " << getTimestamp(_localOplogValue);
            return StatusWith<RollbackCommonPoint>(ErrorCodes::NoMatchingDocument,
                                                   "RS101 reached beginning of local oplog [2]");
        }
        _localOplogValue = result.getValue();
    }

    if (getTimestamp(_localOplogValue) == getTimestamp(operation)) {
        _scanned++;
        if (getHash(_localOplogValue) == getHash(operation)) {
            return StatusWith<RollbackCommonPoint>(
                std::make_pair(getOpTime(_localOplogValue), _localOplogValue.second));
        }

        LOG(2) << "Local oplog entry to roll back: " << redact(_localOplogValue.first);
        auto status = _rollbackOperation(_localOplogValue.first);
        if (!status.isOK()) {
            invariant(ErrorCodes::NoSuchKey != status.code());
            return status;
        }
        auto result = _localOplogIterator->next();
        if (!result.isOK()) {
            severe() << "rollback error RS101 reached beginning of local oplog";
            log() << "    scanned: " << _scanned;
            log() << "  theirTime: " << getTimestamp(operation);
            log() << "  ourTime:   " << getTimestamp(_localOplogValue);
            return StatusWith<RollbackCommonPoint>(ErrorCodes::NoMatchingDocument,
                                                   "RS101 reached beginning of local oplog [1]");
        }
        _localOplogValue = result.getValue();
        return StatusWith<RollbackCommonPoint>(
            ErrorCodes::NoSuchKey,
            "Unable to determine common point - same timestamp but different hash. "
            "Need to process additional remote operations.");
    }

    invariant(getTimestamp(_localOplogValue) < getTimestamp(operation));
    _scanned++;
    return StatusWith<RollbackCommonPoint>(ErrorCodes::NoSuchKey,
                                           "Unable to determine common point. "
                                           "Need to process additional remote operations.");
}

//参考https://mongoing.com/archives/77853
/*
回滚的旧主，需要确认重新选主后，自己的 oplog 历史和新主的 oplog 历史发生“分叉”的时间点，在这个时
间点之前，新主和旧主的 oplog 是一致的，所以这个点也被称之为「common point」。旧主上从「common point」
开始到自己最新的时间点之间的 oplog 就是未来及复制到新主的“多余”部分，需要回滚掉。

common point 的查找逻辑在 syncRollBackLocalOperations() 中实现，大致流程为，由新到老（反向）从
同步源节点获取每条 oplog，然后和自己本地的 oplog 进行比对。本地 oplog 的扫描同样为反向，由于 oplog 的
时间戳可以保证递增，扫描时可以通过保存中间位点的方式来减少重复扫描。如果最终在本地找到一条 oplog 的
时间戳和 term 和同步源的完全一样，那么这条 oplog 即为 common point。由于在分布式环境下，不同节点的
时钟不能做到完全实时同步，而 term 可以唯一标识一个主节点在任期间的修改（oplog）历史，所以需要把
oplog ts 和 term 结合起来进行 common point 的查找。

//3.6版本ts在主和从是完全一样的，所以比较ts即可
*/
StatusWith<RollBackLocalOperations::RollbackCommonPoint> syncRollBackLocalOperations(
    const OplogInterface& localOplog,
    const OplogInterface& remoteOplog,
    const RollBackLocalOperations::RollbackOperationFn& rollbackOperation) {
    auto remoteIterator = remoteOplog.makeIterator();
    auto remoteResult = remoteIterator->next();
    if (!remoteResult.isOK()) {
        return StatusWith<RollBackLocalOperations::RollbackCommonPoint>(
            ErrorCodes::InvalidSyncSource, "remote oplog empty or unreadable");
    }

	//找到需要rollback的公共点
    RollBackLocalOperations finder(localOplog, rollbackOperation);
    Timestamp theirTime;
    while (remoteResult.isOK()) {
        theirTime = remoteResult.getValue().first["ts"].timestamp();
        BSONObj theirObj = remoteResult.getValue().first;
        auto result = finder.onRemoteOperation(theirObj);
        if (result.isOK()) {
            return result.getValue();
        } else if (result.getStatus().code() != ErrorCodes::NoSuchKey) {
            return result;
        }
        remoteResult = remoteIterator->next();
    }

    severe() << "rollback error RS100 reached beginning of remote oplog";
    log() << "  them:      " << remoteOplog.toString();
    log() << "  theirTime: " << theirTime;
    return StatusWith<RollBackLocalOperations::RollbackCommonPoint>(
        ErrorCodes::NoMatchingDocument, "RS100 reached beginning of remote oplog [1]");
}

}  // namespace repl
}  // namespace mongo
