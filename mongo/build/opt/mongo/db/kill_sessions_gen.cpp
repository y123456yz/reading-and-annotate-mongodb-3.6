/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/kill_sessions_gen.h --output build/opt/mongo/db/kill_sessions_gen.cpp src/mongo/db/kill_sessions.idl
 */

#include "mongo/db/kill_sessions_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData KillSessionsCmdFromClient::kKillSessionsFieldName;


KillSessionsCmdFromClient::KillSessionsCmdFromClient() : _hasKillSessions(false) {
    // Used for initialization only
}

KillSessionsCmdFromClient KillSessionsCmdFromClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillSessionsCmdFromClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillSessionsCmdFromClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kKillSessionsFieldName) {
            _hasKillSessions = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kKillSessionsFieldName, &ctxt);
            std::vector<LogicalSessionFromClient> values;

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
                        IDLParserErrorContext tempContext(kKillSessionsFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(LogicalSessionFromClient::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _killSessions = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kKillSessionsFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kKillSessionsFieldName);
    }

}


void KillSessionsCmdFromClient::serialize(BSONObjBuilder* builder) const {
    invariant(_hasKillSessions);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kKillSessionsFieldName));
        for (const auto& item : _killSessions) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj KillSessionsCmdFromClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData KillAllSessionsUser::kDbFieldName;
constexpr StringData KillAllSessionsUser::kUserFieldName;


KillAllSessionsUser::KillAllSessionsUser() : _hasUser(false), _hasDb(false) {
    // Used for initialization only
}

KillAllSessionsUser KillAllSessionsUser::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillAllSessionsUser object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillAllSessionsUser::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kUserBit = 0;
    const size_t kDbBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kUserFieldName) {
            if (MONGO_unlikely(usedFields[kUserBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUserBit);

            _hasUser = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _user = element.str();
            }
        }
        else if (fieldName == kDbFieldName) {
            if (MONGO_unlikely(usedFields[kDbBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbBit);

            _hasDb = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _db = element.str();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kUserBit]) {
            ctxt.throwMissingField(kUserFieldName);
        }
        if (!usedFields[kDbBit]) {
            ctxt.throwMissingField(kDbFieldName);
        }
    }

}


void KillAllSessionsUser::serialize(BSONObjBuilder* builder) const {
    invariant(_hasUser && _hasDb);

    builder->append(kUserFieldName, _user);

    builder->append(kDbFieldName, _db);

}


BSONObj KillAllSessionsUser::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData KillAllSessionsRole::kDbFieldName;
constexpr StringData KillAllSessionsRole::kRoleFieldName;


KillAllSessionsRole::KillAllSessionsRole() : _hasRole(false), _hasDb(false) {
    // Used for initialization only
}

KillAllSessionsRole KillAllSessionsRole::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillAllSessionsRole object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillAllSessionsRole::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kRoleBit = 0;
    const size_t kDbBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kRoleFieldName) {
            if (MONGO_unlikely(usedFields[kRoleBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kRoleBit);

            _hasRole = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _role = element.str();
            }
        }
        else if (fieldName == kDbFieldName) {
            if (MONGO_unlikely(usedFields[kDbBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbBit);

            _hasDb = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _db = element.str();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kRoleBit]) {
            ctxt.throwMissingField(kRoleFieldName);
        }
        if (!usedFields[kDbBit]) {
            ctxt.throwMissingField(kDbFieldName);
        }
    }

}


void KillAllSessionsRole::serialize(BSONObjBuilder* builder) const {
    invariant(_hasRole && _hasDb);

    builder->append(kRoleFieldName, _role);

    builder->append(kDbFieldName, _db);

}


BSONObj KillAllSessionsRole::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData KillAllSessionsCmd::kKillAllSessionsFieldName;


KillAllSessionsCmd::KillAllSessionsCmd() : _hasKillAllSessions(false) {
    // Used for initialization only
}

KillAllSessionsCmd KillAllSessionsCmd::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillAllSessionsCmd object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillAllSessionsCmd::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kKillAllSessionsFieldName) {
            _hasKillAllSessions = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kKillAllSessionsFieldName, &ctxt);
            std::vector<KillAllSessionsUser> values;

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
                        IDLParserErrorContext tempContext(kKillAllSessionsFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(KillAllSessionsUser::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _killAllSessions = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kKillAllSessionsFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kKillAllSessionsFieldName);
    }

}


void KillAllSessionsCmd::serialize(BSONObjBuilder* builder) const {
    invariant(_hasKillAllSessions);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kKillAllSessionsFieldName));
        for (const auto& item : _killAllSessions) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj KillAllSessionsCmd::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData KillAllSessionsByPattern::kLsidFieldName;
constexpr StringData KillAllSessionsByPattern::kRolesFieldName;
constexpr StringData KillAllSessionsByPattern::kUidFieldName;
constexpr StringData KillAllSessionsByPattern::kUsersFieldName;


KillAllSessionsByPattern::KillAllSessionsByPattern()  {
    // Used for initialization only
}

KillAllSessionsByPattern KillAllSessionsByPattern::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillAllSessionsByPattern object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillAllSessionsByPattern::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<4> usedFields;
    const size_t kLsidBit = 0;
    const size_t kUidBit = 1;
    const size_t kUsersBit = 2;
    const size_t kRolesBit = 3;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kLsidFieldName) {
            if (MONGO_unlikely(usedFields[kLsidBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLsidBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kLsidFieldName, &ctxt);
                const auto localObject = element.Obj();
                _lsid = LogicalSessionId::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kUidFieldName) {
            if (MONGO_unlikely(usedFields[kUidBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUidBit);

            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, BinDataGeneral))) {
                _uid = SHA256Block::fromBinData(element._binDataVector());
            }
        }
        else if (fieldName == kUsersFieldName) {
            if (MONGO_unlikely(usedFields[kUsersBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUsersBit);

            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kUsersFieldName, &ctxt);
            std::vector<KillAllSessionsUser> values;

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
                        IDLParserErrorContext tempContext(kUsersFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(KillAllSessionsUser::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _users = std::move(values);
        }
        else if (fieldName == kRolesFieldName) {
            if (MONGO_unlikely(usedFields[kRolesBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kRolesBit);

            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kRolesFieldName, &ctxt);
            std::vector<KillAllSessionsRole> values;

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
                        IDLParserErrorContext tempContext(kRolesFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(KillAllSessionsRole::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _roles = std::move(values);
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
    }

}


void KillAllSessionsByPattern::serialize(BSONObjBuilder* builder) const {
    if (_lsid.is_initialized()) {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kLsidFieldName));
        _lsid.get().serialize(&subObjBuilder);
    }

    if (_uid.is_initialized()) {
        ConstDataRange tempCDR = _uid.get().toCDR();
        builder->append(kUidFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), BinDataGeneral));
    }

    if (_users.is_initialized()) {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kUsersFieldName));
        for (const auto& item : _users.get()) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

    if (_roles.is_initialized()) {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kRolesFieldName));
        for (const auto& item : _roles.get()) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj KillAllSessionsByPattern::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData KillAllSessionsByPatternCmd::kKillAllSessionsByPatternFieldName;


KillAllSessionsByPatternCmd::KillAllSessionsByPatternCmd() : _hasKillAllSessionsByPattern(false) {
    // Used for initialization only
}

KillAllSessionsByPatternCmd KillAllSessionsByPatternCmd::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillAllSessionsByPatternCmd object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillAllSessionsByPatternCmd::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kKillAllSessionsByPatternFieldName) {
            _hasKillAllSessionsByPattern = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kKillAllSessionsByPatternFieldName, &ctxt);
            std::vector<KillAllSessionsByPattern> values;

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
                        IDLParserErrorContext tempContext(kKillAllSessionsByPatternFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(KillAllSessionsByPattern::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _killAllSessionsByPattern = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kKillAllSessionsByPatternFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kKillAllSessionsByPatternFieldName);
    }

}


void KillAllSessionsByPatternCmd::serialize(BSONObjBuilder* builder) const {
    invariant(_hasKillAllSessionsByPattern);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kKillAllSessionsByPatternFieldName));
        for (const auto& item : _killAllSessionsByPattern) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj KillAllSessionsByPatternCmd::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData KillSessionsCmdToServer::kKillSessionsFieldName;


KillSessionsCmdToServer::KillSessionsCmdToServer() : _hasKillSessions(false) {
    // Used for initialization only
}

KillSessionsCmdToServer KillSessionsCmdToServer::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    KillSessionsCmdToServer object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void KillSessionsCmdToServer::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t kKillSessionsBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kKillSessionsFieldName) {
            if (MONGO_unlikely(usedFields[kKillSessionsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kKillSessionsBit);

            _hasKillSessions = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kKillSessionsFieldName, &ctxt);
            std::vector<LogicalSessionId> values;

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
                        IDLParserErrorContext tempContext(kKillSessionsFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(LogicalSessionId::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _killSessions = std::move(values);
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kKillSessionsBit]) {
            ctxt.throwMissingField(kKillSessionsFieldName);
        }
    }

}


void KillSessionsCmdToServer::serialize(BSONObjBuilder* builder) const {
    invariant(_hasKillSessions);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kKillSessionsFieldName));
        for (const auto& item : _killSessions) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj KillSessionsCmdToServer::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
