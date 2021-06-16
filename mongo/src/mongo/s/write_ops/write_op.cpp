/**
 *    Copyright (C) 2013 MongoDB Inc.
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

#include "mongo/s/write_ops/write_op.h"

#include "mongo/util/assert_util.h"

namespace mongo {

using std::stringstream;
using std::vector;

const BatchItemRef& WriteOp::getWriteItem() const {
    return _itemRef;
}

WriteOpState WriteOp::getWriteState() const {
    return _state;
}

const WriteErrorDetail& WriteOp::getOpError() const {
    dassert(_state == WriteOpState_Error);
    return *_error;
}

//BatchWriteOp::targetBatch
//获取该op对应的后端mongod节点TargetedWrite，也就是发送到后端那个分片  应该发送到后端那些mongod
//该批量写操作中的一个writeOp应该转发到指定的targetedWrites对应shard中，这里为数组原因是删除 更新可能对应多个shard
Status WriteOp::targetWrites(OperationContext* opCtx,
                             const NSTargeter& targeter, //ChunkManagerTargeter
                             std::vector<TargetedWrite*>* targetedWrites) {
    bool isUpdate = _itemRef.getOpType() == BatchedCommandRequest::BatchType_Update;
    bool isDelete = _itemRef.getOpType() == BatchedCommandRequest::BatchType_Delete;
    bool isIndexInsert = _itemRef.getRequest()->isInsertIndexRequest();

    Status targetStatus = Status::OK();
    std::vector<std::unique_ptr<ShardEndpoint>> endpoints;

    if (isUpdate) { 
		//ChunkManagerTargeter::targetUpdate
		//获取请求对应的shard和shardVersion信息存入ShardEndpoint
		//批量更新操作中的单个update请求，可能需要转发到多个shard
        targetStatus = targeter.targetUpdate(opCtx, _itemRef.getUpdate(), &endpoints);
    } else if (isDelete) { 
    	//ChunkManagerTargeter::targetDelete
    	//批量删除操作中的单个delete请求，可能需要转发到多个shard
        targetStatus = targeter.targetDelete(opCtx, _itemRef.getDelete(), &endpoints);
    } else { //insert
        dassert(_itemRef.getOpType() == BatchedCommandRequest::BatchType_Insert);

        ShardEndpoint* endpoint = NULL;
        // TODO: Remove the index targeting stuff once there is a command for it
        if (!isIndexInsert) {   
			//ChunkManagerTargeter::targetInsert  
			////获取insert请求对应的shard和shardVersion信息存入ShardEndpoint
            targetStatus = targeter.targetInsert(opCtx, _itemRef.getDocument(), &endpoint);
        } else {//这个分支已经无用了
            // TODO: Retry index writes with stale version?
            //ChunkManagerTargeter::targetCollection
            targetStatus = targeter.targetCollection(&endpoints);
        }

        if (!targetStatus.isOK()) {
            dassert(NULL == endpoint);
            return targetStatus;
        }

        // Store single endpoint result if we targeted a single endpoint
        //批量insert操作，单个insert只会到一个shard，不会到多个
        if (endpoint)
            endpoints.push_back(std::unique_ptr<ShardEndpoint>{endpoint});
    }

    // If we're targeting more than one endpoint with an update/delete, we have to target
    // everywhere since we cannot currently retry partial results.
   // 如果我们使用更新/删除的目标是多个端点，那么我们必须将目标放在所有地方，
   //因为目前我们不能重试部分结果。
   
    // NOTE: Index inserts are currently specially targeted only at the current collection to
    // avoid creating collections everywhere.
    //如果通过请求中的片建信息确定需要发送到多个分片，则清理endpoints信息，把target改为所有分片
    if (targetStatus.isOK() && endpoints.size() > 1u && !isIndexInsert) {
        endpoints.clear();
        invariant(endpoints.empty());
		//获取所有的分片信息添加到endpoints数组
        targetStatus = targeter.targetAllShards(&endpoints); //需要发送给所有shard分片
    }

    // If we had an error, stop here
    if (!targetStatus.isOK())
        return targetStatus;

	//把增删改操作可能转发到多个shard，需要记录
	//insert只会到一个shard, update delete可能到多个shard
    for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        ShardEndpoint* endpoint = it->get();

		//该update insert 或者delete操作对应的ShardEndpoint记录到_childOps数组
		//可能同一个writeOp需要转发到多个shard，所以这里需要一个数组记录，每个shard都对应同一个writeOp数据
        _childOps.emplace_back(this);

		//代表这是该批量操作的第几个操作，并且该操作需要转发到_childOps中记录的第几个分片(_childOps[i]对应一个shard)
		//遇意：批量请求中的第i个操作需要转发到后端第j个shard
		WriteOpRef ref(_itemRef.getItemIndex(), _childOps.size() - 1);

        // For now, multiple endpoints imply no versioning - we can't retry half a multi-write
        if (endpoints.size() == 1u) {
			//该操作只需要转发到一个shard
            targetedWrites->push_back(new TargetedWrite(*endpoint, ref)); //这样要操作的文档WriteOp就和targetedWrites关联起来
        } else {
        	//该操作需要转发到多个shard
            ShardEndpoint broadcastEndpoint(endpoint->shardName, ChunkVersion::IGNORED());
            targetedWrites->push_back(new TargetedWrite(broadcastEndpoint, ref));
        }

        _childOps.back().pendingWrite = targetedWrites->back();
        _childOps.back().state = WriteOpState_Pending;
    }

    _state = WriteOpState_Pending;
    return Status::OK();
}

size_t WriteOp::getNumTargeted() {
    return _childOps.size();
}

static bool isRetryErrCode(int errCode) {
    return errCode == ErrorCodes::StaleShardVersion;
}

// Aggregate a bunch of errors for a single op together
static void combineOpErrors(const vector<ChildWriteOp const*>& errOps, WriteErrorDetail* error) {
    // Special case single response
    if (errOps.size() == 1) {
        errOps.front()->error->cloneTo(error);
        return;
    }

    error->setErrCode(ErrorCodes::MultipleErrorsOccurred);

    // Generate the multi-error message below
    stringstream msg;
    msg << "multiple errors for op : ";

    BSONArrayBuilder errB;
    for (vector<ChildWriteOp const*>::const_iterator it = errOps.begin(); it != errOps.end();
         ++it) {
        const ChildWriteOp* errOp = *it;
        if (it != errOps.begin())
            msg << " :: and :: ";
        msg << errOp->error->getErrMessage();
        errB.append(errOp->error->toBSON());
    }

    error->setErrInfo(BSON("causedBy" << errB.arr()));
    error->setIndex(errOps.front()->error->getIndex());
    error->setErrMessage(msg.str());
}

/**
 * This is the core function which aggregates all the results of a write operation on multiple
 * shards and updates the write operation's state.
 */
//一批数据到代理后，代理转发过程的状态变化过程
void WriteOp::_updateOpState() {
    std::vector<ChildWriteOp const*> childErrors;

    bool isRetryError = true;
    for (const auto& childOp : _childOps) {
        // Don't do anything till we have all the info
        if (childOp.state != WriteOpState_Completed && childOp.state != WriteOpState_Error) {
            return;
        }

        if (childOp.state == WriteOpState_Error) {
            childErrors.push_back(&childOp);

            // Any non-retry error aborts all
            if (!isRetryErrCode(childOp.error->getErrCode())) {
                isRetryError = false;
            }
        }
    }

    if (!childErrors.empty() && isRetryError) {
        // Since we're using broadcast mode for multi-shard writes, which cannot SCE
        invariant(childErrors.size() == 1u);
        _state = WriteOpState_Ready;
    } else if (!childErrors.empty()) {
        _error.reset(new WriteErrorDetail);
        combineOpErrors(childErrors, _error.get());
        _state = WriteOpState_Error;
    } else {
        _state = WriteOpState_Completed;
    }

    invariant(_state != WriteOpState_Pending);
    _childOps.clear();
}

void WriteOp::cancelWrites(const WriteErrorDetail* why) {
    invariant(_state == WriteOpState_Pending || _state == WriteOpState_Ready);

    for (auto& childOp : _childOps) {
        if (childOp.state == WriteOpState_Pending) {
            childOp.endpoint.reset(new ShardEndpoint(childOp.pendingWrite->endpoint));
            if (why) {
                childOp.error.reset(new WriteErrorDetail);
                why->cloneTo(childOp.error.get());
            }

            childOp.state = WriteOpState_Cancelled;
        }
    }

    _state = WriteOpState_Ready;
    _childOps.clear();
}

void WriteOp::noteWriteComplete(const TargetedWrite& targetedWrite) {
    const WriteOpRef& ref = targetedWrite.writeOpRef;
    auto& childOp = _childOps[ref.second];

    childOp.pendingWrite = NULL;
    childOp.endpoint.reset(new ShardEndpoint(targetedWrite.endpoint));
    childOp.state = WriteOpState_Completed;
    _updateOpState();
}

void WriteOp::noteWriteError(const TargetedWrite& targetedWrite, const WriteErrorDetail& error) {
    const WriteOpRef& ref = targetedWrite.writeOpRef;
    auto& childOp = _childOps[ref.second];

    childOp.pendingWrite = NULL;
    childOp.endpoint.reset(new ShardEndpoint(targetedWrite.endpoint));
    childOp.error.reset(new WriteErrorDetail);
    error.cloneTo(childOp.error.get());
    dassert(ref.first == _itemRef.getItemIndex());
    childOp.error->setIndex(_itemRef.getItemIndex());
    childOp.state = WriteOpState_Error;
    _updateOpState();
}

void WriteOp::setOpError(const WriteErrorDetail& error) {
    dassert(_state == WriteOpState_Ready);
    _error.reset(new WriteErrorDetail);
    error.cloneTo(_error.get());
    _error->setIndex(_itemRef.getItemIndex());
    _state = WriteOpState_Error;
    // No need to updateOpState, set directly
}

}  // namespace mongo
