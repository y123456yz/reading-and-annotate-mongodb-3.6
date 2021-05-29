/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/ops/write_ops_gen.h --output build/opt/mongo/db/ops/write_ops_gen.cpp src/mongo/db/ops/write_ops.idl
 */

#include "mongo/db/ops/write_ops_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {
namespace write_ops {
/*
//参考 mongodb字段验证规则（schema validation）
//https://www.cnblogs.com/itxiaoqiang/p/5538287.html   是否验证schema
static constexpr auto kBypassDocumentValidationFieldName = "bypassDocumentValidation"_sd;
//ordered一般针对insert_many bulk_write  这个参数为True时，迫使MongoDB按顺序同步插入数据；
//而如果为False，则MongoDB会并发的不按固定顺序进行批量插入。显然当我们对性能有要求时，
//将该参数设为False是非常必要的。
static constexpr auto kOrderedFieldName = "ordered"_sd;
//对应请求里每个操作（以insert为例，一个insert命令可以插入多个文档）操作ID
static constexpr auto kStmtIdsFieldName = "stmtIds"_sd;
*/

//该文件主要实现从OpMsgRequest中解析出write_ops::Insert write_ops::Update write_ops::Delete类相关成员信息

constexpr StringData WriteCommandBase::kBypassDocumentValidationFieldName;
constexpr StringData WriteCommandBase::kOrderedFieldName;
constexpr StringData WriteCommandBase::kStmtIdsFieldName;


WriteCommandBase::WriteCommandBase()  {
    // Used for initialization only
}

WriteCommandBase WriteCommandBase::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    WriteCommandBase object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void WriteCommandBase::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
		//有重复的elem
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertTypes(element, {Bool, NumberLong, NumberInt, NumberDecimal, NumberDouble}))) {
				//https://www.cnblogs.com/itxiaoqiang/p/5538287.html   是否验证schema
				_bypassDocumentValidation = element.trueValue();
            }
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
				//ordered一般针对insert_many bulk_write  这个参数为True时，迫使MongoDB按顺序同步插入数据；
				//而如果为False，则MongoDB会并发的不按固定顺序进行批量插入。显然当我们对性能有要求时，
				//将该参数设为False是非常必要的。
                _ordered = element.boolean();
            }
        }
        else if (fieldName == kStmtIdsFieldName) {
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kStmtIdsFieldName, &ctxt);
            std::vector<std::int32_t> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, NumberInt)) {
                        values.emplace_back(arrayElement._numberInt());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
			//对应请求里每个操作（以insert为例，一个insert命令可以插入多个文档）操作ID
            _stmtIds = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kBypassDocumentValidationFieldName) == usedFields.end())) {
        _bypassDocumentValidation = false;
    }
    if (MONGO_unlikely(usedFields.find(kOrderedFieldName) == usedFields.end())) {
        _ordered = true;
    }

}

//序列化
void WriteCommandBase::serialize(BSONObjBuilder* builder) const {
    builder->append(kBypassDocumentValidationFieldName, _bypassDocumentValidation);

    builder->append(kOrderedFieldName, _ordered);

    if (_stmtIds.is_initialized()) {
        builder->append(kStmtIdsFieldName, _stmtIds.get());
    }

}

//转换为bson
BSONObj WriteCommandBase::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData UpdateOpEntry::kArrayFiltersFieldName;
constexpr StringData UpdateOpEntry::kCollationFieldName;
constexpr StringData UpdateOpEntry::kMultiFieldName;
constexpr StringData UpdateOpEntry::kQFieldName;
constexpr StringData UpdateOpEntry::kUFieldName;
constexpr StringData UpdateOpEntry::kUpsertFieldName;


UpdateOpEntry::UpdateOpEntry() : _hasQ(false), _hasU(false) {
    // Used for initialization only
}

UpdateOpEntry UpdateOpEntry::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    UpdateOpEntry object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}

/*
 db.collection.update(
   <query>,
   <update>,
   {
     upsert: <boolean>,
     multi: <boolean>,
     writeConcern: <document>,
     collation: <document>,
     arrayFilters: [ <filterdocument1>, ... ]
   }
)
*/
//UpdateOpEntry::parse中调用，从obj中解析除UpdateOpEntry
void UpdateOpEntry::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<6> usedFields;
    const size_t kQBit = 0;
    const size_t kUBit = 1;
    const size_t kArrayFiltersBit = 2;
    const size_t kMultiBit = 3;
    const size_t kUpsertBit = 4;
    const size_t kCollationBit = 5;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kQFieldName) {
            if (MONGO_unlikely(usedFields[kQBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kQBit);

            _hasQ = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _q = element.Obj();
            }
        }
        else if (fieldName == kUFieldName) {
            if (MONGO_unlikely(usedFields[kUBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUBit);

            _hasU = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _u = element.Obj();
            }
        }
        else if (fieldName == kArrayFiltersFieldName) {
            if (MONGO_unlikely(usedFields[kArrayFiltersBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kArrayFiltersBit);

            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kArrayFiltersFieldName, &ctxt);
            std::vector<mongo::BSONObj> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        values.emplace_back(arrayElement.Obj());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _arrayFilters = std::move(values);
        }
        else if (fieldName == kMultiFieldName) {
            if (MONGO_unlikely(usedFields[kMultiBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMultiBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _multi = element.boolean();
            }
        }
        else if (fieldName == kUpsertFieldName) {
            if (MONGO_unlikely(usedFields[kUpsertBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUpsertBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _upsert = element.boolean();
            }
        }
        else if (fieldName == kCollationFieldName) {
            if (MONGO_unlikely(usedFields[kCollationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kCollationBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _collation = element.Obj();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kQBit]) {
            ctxt.throwMissingField(kQFieldName);
        }
        if (!usedFields[kUBit]) {
            ctxt.throwMissingField(kUFieldName);
        }
        if (!usedFields[kMultiBit]) {
            _multi = false;
        }
        if (!usedFields[kUpsertBit]) {
            _upsert = false;
        }
    }

}

//序列化UpdateOpEntry到builder
void UpdateOpEntry::serialize(BSONObjBuilder* builder) const {
    invariant(_hasQ && _hasU);

    builder->append(kQFieldName, _q);

    builder->append(kUFieldName, _u);

    if (_arrayFilters.is_initialized()) {
        builder->append(kArrayFiltersFieldName, _arrayFilters.get());
    }

    builder->append(kMultiFieldName, _multi);

    builder->append(kUpsertFieldName, _upsert);

    if (_collation.is_initialized()) {
        builder->append(kCollationFieldName, _collation.get());
    }

}

//UpdateOpEntry转换为bson
BSONObj UpdateOpEntry::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData DeleteOpEntry::kCollationFieldName;
constexpr StringData DeleteOpEntry::kMultiFieldName;
constexpr StringData DeleteOpEntry::kQFieldName;


DeleteOpEntry::DeleteOpEntry() : _multi(false), _hasQ(false), _hasMulti(false) {
    // Used for initialization only
}

DeleteOpEntry DeleteOpEntry::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    DeleteOpEntry object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void DeleteOpEntry::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<3> usedFields;
    const size_t kQBit = 0;
    const size_t kMultiBit = 1;
    const size_t kCollationBit = 2;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kQFieldName) {
            if (MONGO_unlikely(usedFields[kQBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kQBit);

            _hasQ = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _q = element.Obj();
            }
        }
        else if (fieldName == kMultiFieldName) {
            if (MONGO_unlikely(usedFields[kMultiBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMultiBit);

            _hasMulti = true;
            _multi = write_ops::readMultiDeleteProperty(element);
        }
        else if (fieldName == kCollationFieldName) {
            if (MONGO_unlikely(usedFields[kCollationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kCollationBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _collation = element.Obj();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kQBit]) {
            ctxt.throwMissingField(kQFieldName);
        }
        if (!usedFields[kMultiBit]) {
            ctxt.throwMissingField(kMultiFieldName);
        }
    }

}


void DeleteOpEntry::serialize(BSONObjBuilder* builder) const {
    invariant(_hasQ && _hasMulti);

    builder->append(kQFieldName, _q);

    {
        writeMultiDeleteProperty(_multi, kMultiFieldName, builder);
    }

    if (_collation.is_initialized()) {
        builder->append(kCollationFieldName, _collation.get());
    }

}


BSONObj DeleteOpEntry::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData Insert::kBypassDocumentValidationFieldName;
constexpr StringData Insert::kDbNameFieldName;
constexpr StringData Insert::kDocumentsFieldName;
constexpr StringData Insert::kOrderedFieldName;
constexpr StringData Insert::kStmtIdsFieldName;
constexpr StringData Insert::kWriteCommandBaseFieldName;
constexpr StringData Insert::kCommandName;

const std::vector<StringData> Insert::_knownFields {
    Insert::kBypassDocumentValidationFieldName,
    Insert::kDbNameFieldName,
    Insert::kDocumentsFieldName,
    Insert::kOrderedFieldName,
    Insert::kStmtIdsFieldName,
    Insert::kWriteCommandBaseFieldName,
    Insert::kCommandName,
};

Insert::Insert(const NamespaceString nss) : _nss(std::move(nss)), _dbName(nss.db().toString()), _hasDocuments(false), _hasDbName(true) {
    // Used for initialization only
}

Insert Insert::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    NamespaceString localNS;
    Insert object(localNS);
    object.parseProtected(ctxt, bsonObject);
    return object;
}

//从bsonObject中解析出Insert类成员信息 3.6以下低版本用这个，新版本用opMsg，参考后面的类接口实现
void Insert::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
	//重复性检查
	std::bitset<6> usedFields;
    const size_t kBypassDocumentValidationBit = 0;
    const size_t kOrderedBit = 1;
    const size_t kStmtIdsBit = 2;
    const size_t kDocumentsBit = 3;
    const size_t kDbNameBit = 4;
    BSONElement commandElement;
    bool firstFieldFound = false;

	//解析出insert类的对应成员信息
    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();
       
        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

		//insert参数列表
        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_unlikely(usedFields[kBypassDocumentValidationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBypassDocumentValidationBit);

            // ignore field
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_unlikely(usedFields[kOrderedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOrderedBit);

            // ignore field
        }
        else if (fieldName == kStmtIdsFieldName) {
            if (MONGO_unlikely(usedFields[kStmtIdsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStmtIdsBit);

            // ignore field
        }
		//真正的文档内容
        else if (fieldName == kDocumentsFieldName) {
            if (MONGO_unlikely(usedFields[kDocumentsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDocumentsBit);

            _hasDocuments = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kDocumentsFieldName, &ctxt);
            std::vector<mongo::BSONObj> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        values.emplace_back(arrayElement.Obj());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _documents = std::move(values);
        }
        else if (fieldName == kDbNameFieldName) {
            if (MONGO_unlikely(usedFields[kDbNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbNameBit);

            _hasDbName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _dbName = element.str();
            }
        }
        else {
            if (!Command::isGenericArgument(fieldName)) {
                ctxt.throwUnknownField(fieldName);
            }
        }
    }

	//从bsonObject中解析出_writeCommandBase成员内容
    _writeCommandBase = WriteCommandBase::parse(ctxt, bsonObject);

	//判断是否有重复的
    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kDocumentsBit]) {
            ctxt.throwMissingField(kDocumentsFieldName);
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
	//根据db+collection构造出db.collection字符串
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

//从request中解析出Insert类成员信息,3.6版本开始使用opMsg
Insert Insert::parse(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    NamespaceString localNS;
    Insert object(localNS);
    object.parseProtected(ctxt, request);
    return object;
}

//从request中解析出Insert类成员信息,3.6版本开始使用opMsg
void Insert::parseProtected(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    std::bitset<6> usedFields;
    const size_t kBypassDocumentValidationBit = 0;
    const size_t kOrderedBit = 1;
    const size_t kStmtIdsBit = 2;
    const size_t kDocumentsBit = 3;
    const size_t kDbNameBit = 4;
    BSONElement commandElement;
    bool firstFieldFound = false;

	//文档内容以外的选项解析
    for (const auto& element :request.body) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_unlikely(usedFields[kBypassDocumentValidationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBypassDocumentValidationBit);

            // ignore field
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_unlikely(usedFields[kOrderedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOrderedBit);

            // ignore field
        }
        else if (fieldName == kStmtIdsFieldName) {
            if (MONGO_unlikely(usedFields[kStmtIdsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStmtIdsBit);

            // ignore field
        }
        else if (fieldName == kDocumentsFieldName) {
            if (MONGO_unlikely(usedFields[kDocumentsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDocumentsBit);

            _hasDocuments = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kDocumentsFieldName, &ctxt);
            std::vector<mongo::BSONObj> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        values.emplace_back(arrayElement.Obj());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _documents = std::move(values);
        }
        else if (fieldName == kDbNameFieldName) {
            if (MONGO_unlikely(usedFields[kDbNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbNameBit);

            _hasDbName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _dbName = element.str();
            }
        }
        else {
            if (!Command::isGenericArgument(fieldName)) {
                ctxt.throwUnknownField(fieldName);
            }
        }
    }
	//从request中解析出_writeCommandBase成员内容
    _writeCommandBase = WriteCommandBase::parse(ctxt, request.body);

	//sequences信息解析
    for (auto&& sequence : request.sequences) {

        if (sequence.name == kDocumentsFieldName) {
            if (MONGO_unlikely(usedFields[kDocumentsBit])) {
                ctxt.throwDuplicateField(sequence.name);
            }

            usedFields.set(kDocumentsBit);

            _hasDocuments = true;
            std::vector<mongo::BSONObj> values;

            for (const BSONObj& sequenceObject : sequence.objs) {
                values.emplace_back(sequenceObject);
            }
            _documents = std::move(values);
        }
        else {
            ctxt.throwUnknownField(sequence.name);
        }
    }

	//去重判断
    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kDocumentsBit]) {
            ctxt.throwMissingField(kDocumentsFieldName);
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
	//根据db+collection构造出db.collection字符串
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

void Insert::serialize(const BSONObj& commandPassthroughFields, BSONObjBuilder* builder) const {
    invariant(_hasDocuments && _hasDbName);

    invariant(!_nss.isEmpty());
    builder->append("insert", _nss.coll());

    {
        _writeCommandBase.serialize(builder);
    }

    {
        builder->append(kDocumentsFieldName, _documents);
    }

    IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

}

OpMsgRequest Insert::serialize(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder localBuilder;
    {
        BSONObjBuilder* builder = &localBuilder;
        invariant(_hasDocuments && _hasDbName);

        invariant(!_nss.isEmpty());
        builder->append("insert", _nss.coll());

        {
            _writeCommandBase.serialize(builder);
        }

        builder->append(kDbNameFieldName, _dbName);

        IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

    }
    OpMsgRequest request;
    request.body = localBuilder.obj();
    {
        OpMsg::DocumentSequence documentSequence;
        documentSequence.name = kDocumentsFieldName.toString();
        for (const auto& item : _documents) {
            documentSequence.objs.push_back(item);
        }
        request.sequences.emplace_back(documentSequence);
    }

    return request;
}

BSONObj Insert::toBSON(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder builder;
    serialize(commandPassthroughFields, &builder);
    return builder.obj();
}

constexpr StringData Update::kBypassDocumentValidationFieldName;
constexpr StringData Update::kDbNameFieldName;
constexpr StringData Update::kOrderedFieldName;
constexpr StringData Update::kStmtIdsFieldName;
constexpr StringData Update::kUpdatesFieldName;
constexpr StringData Update::kWriteCommandBaseFieldName;
constexpr StringData Update::kCommandName;

const std::vector<StringData> Update::_knownFields {
    Update::kBypassDocumentValidationFieldName,
    Update::kDbNameFieldName,
    Update::kOrderedFieldName,
    Update::kStmtIdsFieldName,
    Update::kUpdatesFieldName,
    Update::kWriteCommandBaseFieldName,
    Update::kCommandName,
};

//Update构造初始化
Update::Update(const NamespaceString nss) : _nss(std::move(nss)), _dbName(nss.db().toString()), _hasUpdates(false), _hasDbName(true) {
    // Used for initialization only
}

//从bsonObject中解析除Update成员信息
Update Update::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    NamespaceString localNS;
    Update object(localNS);
	//Update::parseProtected
    object.parseProtected(ctxt, bsonObject);
    return object;
}

//上面的Update::parse调用解析
//从bsonObject中解析除Update成员信息
void Update::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<6> usedFields;
    const size_t kBypassDocumentValidationBit = 0;
    const size_t kOrderedBit = 1;
    const size_t kStmtIdsBit = 2;
    const size_t kUpdatesBit = 3;
    const size_t kDbNameBit = 4;
    BSONElement commandElement;
    bool firstFieldFound = false;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_unlikely(usedFields[kBypassDocumentValidationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBypassDocumentValidationBit);

            // ignore field
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_unlikely(usedFields[kOrderedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOrderedBit);

            // ignore field
        }
        else if (fieldName == kStmtIdsFieldName) {
            if (MONGO_unlikely(usedFields[kStmtIdsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStmtIdsBit);

            // ignore field
        }
        else if (fieldName == kUpdatesFieldName) {
            if (MONGO_unlikely(usedFields[kUpdatesBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUpdatesBit);

            _hasUpdates = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kUpdatesFieldName, &ctxt);
            std::vector<UpdateOpEntry> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        IDLParserErrorContext tempContext(kUpdatesFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(UpdateOpEntry::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _updates = std::move(values);
        }
        else if (fieldName == kDbNameFieldName) {
            if (MONGO_unlikely(usedFields[kDbNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbNameBit);

            _hasDbName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _dbName = element.str();
            }
        }
        else {
            if (!Command::isGenericArgument(fieldName)) {
                ctxt.throwUnknownField(fieldName);
            }
        }
    }
    _writeCommandBase = WriteCommandBase::parse(ctxt, bsonObject);


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kUpdatesBit]) {
            ctxt.throwMissingField(kUpdatesFieldName);
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

Update Update::parse(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    NamespaceString localNS;
    Update object(localNS);
    object.parseProtected(ctxt, request);
    return object;
}

//从OpMsgRequest中解析除Update信息
void Update::parseProtected(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    std::bitset<6> usedFields;
    const size_t kBypassDocumentValidationBit = 0;
    const size_t kOrderedBit = 1;
    const size_t kStmtIdsBit = 2;
    const size_t kUpdatesBit = 3;
    const size_t kDbNameBit = 4;
    BSONElement commandElement;
    bool firstFieldFound = false;

    for (const auto& element :request.body) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_unlikely(usedFields[kBypassDocumentValidationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBypassDocumentValidationBit);

            // ignore field
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_unlikely(usedFields[kOrderedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOrderedBit);

            // ignore field
        }
        else if (fieldName == kStmtIdsFieldName) {
            if (MONGO_unlikely(usedFields[kStmtIdsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStmtIdsBit);

            // ignore field
        }
        else if (fieldName == kUpdatesFieldName) {
            if (MONGO_unlikely(usedFields[kUpdatesBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUpdatesBit);

            _hasUpdates = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kUpdatesFieldName, &ctxt);
            std::vector<UpdateOpEntry> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        IDLParserErrorContext tempContext(kUpdatesFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(UpdateOpEntry::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _updates = std::move(values);
        }
        else if (fieldName == kDbNameFieldName) {
            if (MONGO_unlikely(usedFields[kDbNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbNameBit);

            _hasDbName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _dbName = element.str();
            }
        }
        else {
            if (!Command::isGenericArgument(fieldName)) {
                ctxt.throwUnknownField(fieldName);
            }
        }
    }
    _writeCommandBase = WriteCommandBase::parse(ctxt, request.body);


    for (auto&& sequence : request.sequences) {

        if (sequence.name == kUpdatesFieldName) {
            if (MONGO_unlikely(usedFields[kUpdatesBit])) {
                ctxt.throwDuplicateField(sequence.name);
            }

            usedFields.set(kUpdatesBit);

            _hasUpdates = true;
            std::vector<UpdateOpEntry> values;

            for (const BSONObj& sequenceObject : sequence.objs) {
                IDLParserErrorContext tempContext(kUpdatesFieldName, &ctxt);
                values.emplace_back(UpdateOpEntry::parse(tempContext, sequenceObject));
            }
            _updates = std::move(values);
        }
        else {
            ctxt.throwUnknownField(sequence.name);
        }
    }

    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kUpdatesBit]) {
            ctxt.throwMissingField(kUpdatesFieldName);
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

//把update和commandPassthroughFields序列号到
void Update::serialize(const BSONObj& commandPassthroughFields, BSONObjBuilder* builder) const {
    invariant(_hasUpdates && _hasDbName);

    invariant(!_nss.isEmpty());
    builder->append("update", _nss.coll());

    {
		//_writeCommandBase序列化
        _writeCommandBase.serialize(builder);
    }

    {	
		//_updates序列化
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kUpdatesFieldName));
        for (const auto& item : _updates) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }
	
	//commandPassthroughFields序列化到builder
    IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

}

OpMsgRequest Update::serialize(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder localBuilder;
    {
        BSONObjBuilder* builder = &localBuilder;
        invariant(_hasUpdates && _hasDbName);

        invariant(!_nss.isEmpty());
        builder->append("update", _nss.coll());

        {
            _writeCommandBase.serialize(builder);
        }

        builder->append(kDbNameFieldName, _dbName);

        IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

    }
    OpMsgRequest request;
    request.body = localBuilder.obj();
    {
        OpMsg::DocumentSequence documentSequence;
        documentSequence.name = kUpdatesFieldName.toString();
        for (const auto& item : _updates) {
            BSONObjBuilder builder;
            item.serialize(&builder);
            documentSequence.objs.push_back(builder.obj());
        }
        request.sequences.emplace_back(documentSequence);
    }

    return request;
}

BSONObj Update::toBSON(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder builder;
    serialize(commandPassthroughFields, &builder);
    return builder.obj();
}

constexpr StringData Delete::kBypassDocumentValidationFieldName;
constexpr StringData Delete::kDbNameFieldName;
constexpr StringData Delete::kDeletesFieldName;
constexpr StringData Delete::kOrderedFieldName;
constexpr StringData Delete::kStmtIdsFieldName;
constexpr StringData Delete::kWriteCommandBaseFieldName;
constexpr StringData Delete::kCommandName;

const std::vector<StringData> Delete::_knownFields {
    Delete::kBypassDocumentValidationFieldName,
    Delete::kDbNameFieldName,
    Delete::kDeletesFieldName,
    Delete::kOrderedFieldName,
    Delete::kStmtIdsFieldName,
    Delete::kWriteCommandBaseFieldName,
    Delete::kCommandName,
};

Delete::Delete(const NamespaceString nss) : _nss(std::move(nss)), _dbName(nss.db().toString()), _hasDeletes(false), _hasDbName(true) {
    // Used for initialization only
}

Delete Delete::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    NamespaceString localNS;
    Delete object(localNS);
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void Delete::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<6> usedFields;
    const size_t kBypassDocumentValidationBit = 0;
    const size_t kOrderedBit = 1;
    const size_t kStmtIdsBit = 2;
    const size_t kDeletesBit = 3;
    const size_t kDbNameBit = 4;
    BSONElement commandElement;
    bool firstFieldFound = false;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_unlikely(usedFields[kBypassDocumentValidationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBypassDocumentValidationBit);

            // ignore field
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_unlikely(usedFields[kOrderedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOrderedBit);

            // ignore field
        }
        else if (fieldName == kStmtIdsFieldName) {
            if (MONGO_unlikely(usedFields[kStmtIdsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStmtIdsBit);

            // ignore field
        }
        else if (fieldName == kDeletesFieldName) {
            if (MONGO_unlikely(usedFields[kDeletesBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDeletesBit);

            _hasDeletes = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kDeletesFieldName, &ctxt);
            std::vector<DeleteOpEntry> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        IDLParserErrorContext tempContext(kDeletesFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(DeleteOpEntry::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _deletes = std::move(values);
        }
        else if (fieldName == kDbNameFieldName) {
            if (MONGO_unlikely(usedFields[kDbNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbNameBit);

            _hasDbName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _dbName = element.str();
            }
        }
        else {
            if (!Command::isGenericArgument(fieldName)) {
                ctxt.throwUnknownField(fieldName);
            }
        }
    }
    _writeCommandBase = WriteCommandBase::parse(ctxt, bsonObject);


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kDeletesBit]) {
            ctxt.throwMissingField(kDeletesFieldName);
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

Delete Delete::parse(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    NamespaceString localNS;
    Delete object(localNS);
    object.parseProtected(ctxt, request);
    return object;
}
void Delete::parseProtected(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    std::bitset<6> usedFields;
    const size_t kBypassDocumentValidationBit = 0;
    const size_t kOrderedBit = 1;
    const size_t kStmtIdsBit = 2;
    const size_t kDeletesBit = 3;
    const size_t kDbNameBit = 4;
    BSONElement commandElement;
    bool firstFieldFound = false;

    for (const auto& element :request.body) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kBypassDocumentValidationFieldName) {
            if (MONGO_unlikely(usedFields[kBypassDocumentValidationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBypassDocumentValidationBit);

            // ignore field
        }
        else if (fieldName == kOrderedFieldName) {
            if (MONGO_unlikely(usedFields[kOrderedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOrderedBit);

            // ignore field
        }
        else if (fieldName == kStmtIdsFieldName) {
            if (MONGO_unlikely(usedFields[kStmtIdsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStmtIdsBit);

            // ignore field
        }
        else if (fieldName == kDeletesFieldName) {
            if (MONGO_unlikely(usedFields[kDeletesBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDeletesBit);

            _hasDeletes = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kDeletesFieldName, &ctxt);
            std::vector<DeleteOpEntry> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        IDLParserErrorContext tempContext(kDeletesFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(DeleteOpEntry::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _deletes = std::move(values);
        }
        else if (fieldName == kDbNameFieldName) {
            if (MONGO_unlikely(usedFields[kDbNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbNameBit);

            _hasDbName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _dbName = element.str();
            }
        }
        else {
            if (!Command::isGenericArgument(fieldName)) {
                ctxt.throwUnknownField(fieldName);
            }
        }
    }
    _writeCommandBase = WriteCommandBase::parse(ctxt, request.body);


    for (auto&& sequence : request.sequences) {

        if (sequence.name == kDeletesFieldName) {
            if (MONGO_unlikely(usedFields[kDeletesBit])) {
                ctxt.throwDuplicateField(sequence.name);
            }

            usedFields.set(kDeletesBit);

            _hasDeletes = true;
            std::vector<DeleteOpEntry> values;

            for (const BSONObj& sequenceObject : sequence.objs) {
                IDLParserErrorContext tempContext(kDeletesFieldName, &ctxt);
                values.emplace_back(DeleteOpEntry::parse(tempContext, sequenceObject));
            }
            _deletes = std::move(values);
        }
        else {
            ctxt.throwUnknownField(sequence.name);
        }
    }

    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kDeletesBit]) {
            ctxt.throwMissingField(kDeletesFieldName);
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

void Delete::serialize(const BSONObj& commandPassthroughFields, BSONObjBuilder* builder) const {
    invariant(_hasDeletes && _hasDbName);

    invariant(!_nss.isEmpty());
    builder->append("delete", _nss.coll());

    {
        _writeCommandBase.serialize(builder);
    }

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kDeletesFieldName));
        for (const auto& item : _deletes) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

    IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

}

OpMsgRequest Delete::serialize(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder localBuilder;
    {
        BSONObjBuilder* builder = &localBuilder;
        invariant(_hasDeletes && _hasDbName);

        invariant(!_nss.isEmpty());
        builder->append("delete", _nss.coll());

        {
            _writeCommandBase.serialize(builder);
        }

        builder->append(kDbNameFieldName, _dbName);

        IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

    }
    OpMsgRequest request;
    request.body = localBuilder.obj();
    {
        OpMsg::DocumentSequence documentSequence;
        documentSequence.name = kDeletesFieldName.toString();
        for (const auto& item : _deletes) {
            BSONObjBuilder builder;
            item.serialize(&builder);
            documentSequence.objs.push_back(builder.obj());
        }
        request.sequences.emplace_back(documentSequence);
    }

    return request;
}

BSONObj Delete::toBSON(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder builder;
    serialize(commandPassthroughFields, &builder);
    return builder.obj();
}

}  // namespace write_ops
}  // namespace mongo
