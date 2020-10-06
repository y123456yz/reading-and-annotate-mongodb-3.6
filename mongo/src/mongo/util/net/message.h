/**
 *    Copyright (C) 2017 MongoDB Inc.
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

#include <cstdint>

#include "mongo/base/data_type_endian.h"
#include "mongo/base/data_view.h"
#include "mongo/base/encoded_value_storage.h"
#include "mongo/base/static_assert.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

/**
 * Maximum accepted message size on the wire protocol.
 mongodb协议官方文档参考https://docs.mongodb.com/manual/reference/mongodb-wire-protocol/
 */
const size_t MaxMessageSizeBytes = 48 * 1000 * 1000;
//ServiceEntryPointMongod::handleRequest
//enum NetworkOp : int32_t {  //LogicalOp和NetworkOp的转换见NetworkOp
enum NetworkOp { 

    opInvalid = 0,
    opReply = 1,     /* reply. responseTo is set. */
    dbUpdate = 2001, /* update object */
    dbInsert = 2002,
    // dbGetByOID = 2003,
    dbQuery = 2004,
    dbGetMore = 2005,
    dbDelete = 2006,
    dbKillCursors = 2007,
    // dbCommand_DEPRECATED = 2008, 
    // dbCommandReply_DEPRECATED = 2009, 
    
    dbCommand = 2010,
    
    dbCommandReply = 2011,
    dbCompressed = 2012,
    //getProtoString
    dbMsg = 2013,  //3.6版本实际上insert find都是走的该op，参考ServiceEntryPointMongod::handleRequest
};

//检查op是否在NetworkOp范围内，如果不在，说明不支持
inline bool isSupportedRequestNetworkOp(NetworkOp op) {
    switch (op) {
        case dbUpdate:
        case dbInsert:
        case dbQuery:
        case dbGetMore:
        case dbDelete:
        case dbKillCursors:
        case dbCommand:
        case dbCompressed:
        case dbMsg:
            return true;
        case dbCommandReply:
        case opReply:
        default:
            return false;
    }
}

//赋值见networkOpToLogicalOp
enum class LogicalOp {
    opInvalid,
    opUpdate,
    opInsert,  //插入操作在performInserts中使用
    opQuery,
    opGetMore,
    opDelete,
    opKillCursors,
    opCommand,
    opCompressed,
};

inline LogicalOp networkOpToLogicalOp(NetworkOp networkOp) {
    switch (networkOp) {
        case dbUpdate:
            return LogicalOp::opUpdate;
        case dbInsert:
            return LogicalOp::opInsert;
        case dbQuery:
            return LogicalOp::opQuery;
        case dbGetMore:
            return LogicalOp::opGetMore;
        case dbDelete:
            return LogicalOp::opDelete;
        case dbKillCursors:
            return LogicalOp::opKillCursors;
        case dbMsg:
        case dbCommand:
            return LogicalOp::opCommand;
        case dbCompressed:
            return LogicalOp::opCompressed;
        default:
            int op = int(networkOp);
            massert(34348, str::stream() << "cannot translate opcode " << op, !op);
            return LogicalOp::opInvalid;
    }
}

inline const char* networkOpToString(NetworkOp networkOp) {
    switch (networkOp) {
        case opInvalid:
            return "none";
        case opReply:
            return "reply";
        case dbUpdate:
            return "update";
        case dbInsert:
            return "insert";
        case dbQuery:
            return "query";
        case dbGetMore:
            return "getmore";
        case dbDelete:
            return "remove";
        case dbKillCursors:
            return "killcursors";
        case dbCommand:
            return "command";
        case dbCommandReply:
            return "commandReply";
        case dbCompressed:
            return "compressed";
        case dbMsg:
            return "msg";
        default:
            int op = static_cast<int>(networkOp);
            massert(16141, str::stream() << "cannot translate opcode " << op, !op);
            return "";
    }
}

//不同op
inline const char* logicalOpToString(LogicalOp logicalOp) {
    switch (logicalOp) {
        case LogicalOp::opInvalid:
            return "none";
        case LogicalOp::opUpdate:
            return "update";
        case LogicalOp::opInsert:
            return "insert";
        case LogicalOp::opQuery:
            return "query";
        case LogicalOp::opGetMore:
            return "getmore";
        case LogicalOp::opDelete:
            return "remove";
        case LogicalOp::opKillCursors:
            return "killcursors";
        case LogicalOp::opCommand:
            return "command";
        case LogicalOp::opCompressed:
            return "compressed";
        default:
            MONGO_UNREACHABLE;
    }
}

namespace MSGHEADER {

#pragma pack(1)
/**  头部读取参考:TransportLayerASIO::ASIOSourceTicket::fillImpl
 * See http://dochub.mongodb.org/core/mongowireprotocol
 */ //MSGHEADER::Layout
struct Layout {
    //getMessageLength解析
    int32_t messageLength;  // total message size, including this
    //getRequestMsgId解析
    int32_t requestID;      // identifier for this message
    //getResponseToMsgId解析
    int32_t responseTo;     // requestID from the original request
    //   (used in responses from db)
    //getOpCode解析
    int32_t opCode;
};
#pragma pack()

class ConstView { //MSGHEADER::Layout头部字段解析
public:
    typedef ConstDataView view_type;

    ConstView(const char* data) : _data(data) {}

    const char* view2ptr() const {
        return data().view();
    }

    //TransportLayerASIO::ASIOSourceTicket::_headerCallback
    int32_t getMessageLength() const {
        return data().read<LittleEndian<int32_t>>(offsetof(Layout, messageLength));
    }

    int32_t getRequestMsgId() const {
        return data().read<LittleEndian<int32_t>>(offsetof(Layout, requestID));
    }

    int32_t getResponseToMsgId() const {
        return data().read<LittleEndian<int32_t>>(offsetof(Layout, responseTo));
    }

    int32_t getOpCode() const {
        return data().read<LittleEndian<int32_t>>(offsetof(Layout, opCode));
    }

protected:
    const view_type& data() const {
        return _data;
    }

private:
    view_type _data;
};

class View : public ConstView {
public:
    typedef DataView view_type;

    View(char* data) : ConstView(data) {}

    using ConstView::view2ptr;
    char* view2ptr() {
        return data().view();
    }

    //以下四个接口进行header赋值
    void setMessageLength(int32_t value) {
        data().write(tagLittleEndian(value), offsetof(Layout, messageLength));
    }

    void setRequestMsgId(int32_t value) {
        data().write(tagLittleEndian(value), offsetof(Layout, requestID));
    }

    void setResponseToMsgId(int32_t value) {
        data().write(tagLittleEndian(value), offsetof(Layout, responseTo));
    }

    void setOpCode(int32_t value) {
        data().write(tagLittleEndian(value), offsetof(Layout, opCode));
    }

private:
    //指向header起始地址
    view_type data() const {
        return const_cast<char*>(ConstView::view2ptr());
    }
};

class Value : public EncodedValueStorage<Layout, ConstView, View> {
public:
    Value() {
        MONGO_STATIC_ASSERT(sizeof(Value) == sizeof(Layout));
    }

    Value(ZeroInitTag_t zit) : EncodedValueStorage<Layout, ConstView, View>(zit) {}
};

}  // namespace MSGHEADER

namespace MsgData {

#pragma pack(1)
struct Layout {
    MSGHEADER::Layout header;
    char data[4];
};
#pragma pack()

class ConstView {
public:
    ConstView(const char* storage) : _storage(storage) {}

    const char* view2ptr() const {
        return storage().view();
    }

    //获取msg header的值
    int32_t getLen() const {
        return header().getMessageLength();
    }

    int32_t getId() const {
        return header().getRequestMsgId();
    }

    int32_t getResponseToMsgId() const {
        return header().getResponseToMsgId();
    }

    //获取网络数据报文中的op字段
    NetworkOp getNetworkOp() const {
        return NetworkOp(header().getOpCode());
    }

    const char* data() const {
        return storage().view(offsetof(Layout, data));
    }

    bool valid() const {
        if (getLen() <= 0 || getLen() > (4 * BSONObjMaxInternalSize))
            return false;
        if (getNetworkOp() < 0 || getNetworkOp() > 30000)
            return false;
        return true;
    }

    int64_t getCursor() const {
        verify(getResponseToMsgId() > 0);
        verify(getNetworkOp() == opReply);
        return ConstDataView(data() + sizeof(int32_t)).read<LittleEndian<int64_t>>();
    }

    int dataLen() const;  // len without header

protected:
    const ConstDataView& storage() const {
        return _storage;
    }

    //指向header起始地址
    MSGHEADER::ConstView header() const {
        return storage().view(offsetof(Layout, header));
    }

private:
    ConstDataView _storage;
};

class View : public ConstView {
public:
    View(char* storage) : ConstView(storage) {}

    using ConstView::view2ptr;
    char* view2ptr() {
        return storage().view();
    }

    //以下四个接口完成msg header赋值
    void setLen(int value) {
        return header().setMessageLength(value);
    }

    void setId(int32_t value) {
        return header().setRequestMsgId(value);
    }

    void setResponseToMsgId(int32_t value) {
        return header().setResponseToMsgId(value);
    }

    void setOperation(int value) {
        return header().setOpCode(value);
    }

    using ConstView::data;
    char* data() {
        return storage().view(offsetof(Layout, data));
    }

private:
    DataView storage() const {
        return const_cast<char*>(ConstView::view2ptr());
    }

    MSGHEADER::View header() const {
        return storage().view(offsetof(Layout, header));
    }
};

class Value : public EncodedValueStorage<Layout, ConstView, View> {
public:
    Value() {
        MONGO_STATIC_ASSERT(sizeof(Value) == sizeof(Layout));
    }

    Value(ZeroInitTag_t zit) : EncodedValueStorage<Layout, ConstView, View>(zit) {}
};

const int MsgDataHeaderSize = sizeof(Value) - 4;

inline int ConstView::dataLen() const { //除去头部后的数据部分
    return getLen() - MsgDataHeaderSize;
}

}  // namespace MsgData

//DbMessage._msg包含该类成员  message和OpMsgRequest ReplyInterface  ReplyBuilderInterface等的关系可以参考factory.cpp实现
class Message {
public:
    Message() = default;
    explicit Message(SharedBuffer data) : _buf(std::move(data)) {}

    MsgData::View header() const {
        verify(!empty());
        return _buf.get();
    }

    //获取网络数据报文中的op字段 opMsgRequestFromAnyProtocol调用
    NetworkOp operation() const {
        return header().getNetworkOp();
    }

    MsgData::View singleData() const {
        massert(13273, "single data buffer expected", _buf);
        return header();
    }

    bool empty() const {
        return !_buf;
    }

    int size() const {
        if (_buf) {
            return MsgData::ConstView(_buf.get()).getLen();
        }
        return 0;
    }

    int dataSize() const {
        return size() - sizeof(MSGHEADER::Value);
    }

    void reset() {
        _buf = {};
    }

    // use to set first buffer if empty
    //_buf直接使用buf空间
    void setData(SharedBuffer buf) {
        verify(empty());
        _buf = std::move(buf);
    }
     //把msgtxt拷贝到_buf中
    void setData(int operation, const char* msgtxt) {
        setData(operation, msgtxt, strlen(msgtxt) + 1);
    }
    //把msgdata拷贝到_buf中
    void setData(int operation, const char* msgdata, size_t len) {
        verify(empty());
        size_t dataLen = len + sizeof(MsgData::Value) - 4;
        _buf = SharedBuffer::allocate(dataLen);
        MsgData::View d = _buf.get();
        if (len)
            memcpy(d.data(), msgdata, len);
        d.setLen(dataLen);
        d.setOperation(operation);
    }

    char* buf() {
        return _buf.get();
    }

    const char* buf() const {
        return _buf.get();
    }

    SharedBuffer sharedBuffer() {
        return _buf;
    }

    ConstSharedBuffer sharedBuffer() const {
        return _buf;
    }

private:
    //存放接收数据的buf
    SharedBuffer _buf;
};

/**
 * Returns an always incrementing value to be used to assign to the next received network message.
 */
int32_t nextMessageId();

}  // namespace mongo
