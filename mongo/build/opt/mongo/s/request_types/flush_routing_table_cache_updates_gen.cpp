/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/request_types/flush_routing_table_cache_updates_gen.h --output build/opt/mongo/s/request_types/flush_routing_table_cache_updates_gen.cpp src/mongo/s/request_types/flush_routing_table_cache_updates.idl
 */

#include "mongo/s/request_types/flush_routing_table_cache_updates_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData _flushRoutingTableCacheUpdatesRequest::kDbNameFieldName;
constexpr StringData _flushRoutingTableCacheUpdatesRequest::kSyncFromConfigFieldName;
constexpr StringData _flushRoutingTableCacheUpdatesRequest::kCommandName;

const std::vector<StringData> _flushRoutingTableCacheUpdatesRequest::_knownFields {
    _flushRoutingTableCacheUpdatesRequest::kDbNameFieldName,
    _flushRoutingTableCacheUpdatesRequest::kSyncFromConfigFieldName,
    _flushRoutingTableCacheUpdatesRequest::kCommandName,
};

_flushRoutingTableCacheUpdatesRequest::_flushRoutingTableCacheUpdatesRequest(const NamespaceString nss) : _nss(std::move(nss)), _dbName(nss.db().toString()), _hasDbName(true) {
    // Used for initialization only
}

_flushRoutingTableCacheUpdatesRequest _flushRoutingTableCacheUpdatesRequest::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    NamespaceString localNS;
    _flushRoutingTableCacheUpdatesRequest object(localNS);
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void _flushRoutingTableCacheUpdatesRequest::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kSyncFromConfigBit = 0;
    const size_t kDbNameBit = 1;
    BSONElement commandElement;
    bool firstFieldFound = false;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kSyncFromConfigFieldName) {
            if (MONGO_unlikely(usedFields[kSyncFromConfigBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSyncFromConfigBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _syncFromConfig = element.boolean();
            }
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


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kSyncFromConfigBit]) {
            _syncFromConfig = true;
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

_flushRoutingTableCacheUpdatesRequest _flushRoutingTableCacheUpdatesRequest::parse(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    NamespaceString localNS;
    _flushRoutingTableCacheUpdatesRequest object(localNS);
    object.parseProtected(ctxt, request);
    return object;
}
void _flushRoutingTableCacheUpdatesRequest::parseProtected(const IDLParserErrorContext& ctxt, const OpMsgRequest& request) {
    std::bitset<2> usedFields;
    const size_t kSyncFromConfigBit = 0;
    const size_t kDbNameBit = 1;
    BSONElement commandElement;
    bool firstFieldFound = false;

    for (const auto& element :request.body) {
        const auto fieldName = element.fieldNameStringData();

        if (firstFieldFound == false) {
            commandElement = element;
            firstFieldFound = true;
            continue;
        }

        if (fieldName == kSyncFromConfigFieldName) {
            if (MONGO_unlikely(usedFields[kSyncFromConfigBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSyncFromConfigBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _syncFromConfig = element.boolean();
            }
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


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kSyncFromConfigBit]) {
            _syncFromConfig = true;
        }
        if (!usedFields[kDbNameBit]) {
            ctxt.throwMissingField(kDbNameFieldName);
        }
    }

    invariant(_nss.isEmpty());
    _nss = ctxt.parseNSCollectionRequired(_dbName, commandElement);
}

void _flushRoutingTableCacheUpdatesRequest::serialize(const BSONObj& commandPassthroughFields, BSONObjBuilder* builder) const {
    invariant(_hasDbName);

    invariant(!_nss.isEmpty());
    builder->append("_flushRoutingTableCacheUpdatesRequest", _nss.coll());

    builder->append(kSyncFromConfigFieldName, _syncFromConfig);

    IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

}

OpMsgRequest _flushRoutingTableCacheUpdatesRequest::serialize(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder localBuilder;
    {
        BSONObjBuilder* builder = &localBuilder;
        invariant(_hasDbName);

        invariant(!_nss.isEmpty());
        builder->append("_flushRoutingTableCacheUpdatesRequest", _nss.coll());

        builder->append(kSyncFromConfigFieldName, _syncFromConfig);

        builder->append(kDbNameFieldName, _dbName);

        IDLParserErrorContext::appendGenericCommandArguments(commandPassthroughFields, _knownFields, builder);

    }
    OpMsgRequest request;
    request.body = localBuilder.obj();
    return request;
}

BSONObj _flushRoutingTableCacheUpdatesRequest::toBSON(const BSONObj& commandPassthroughFields) const {
    BSONObjBuilder builder;
    serialize(commandPassthroughFields, &builder);
    return builder.obj();
}

}  // namespace mongo
