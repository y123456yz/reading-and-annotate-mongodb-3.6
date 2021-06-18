/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/logical_session_cache_stats_gen.h --output build/opt/mongo/db/logical_session_cache_stats_gen.cpp src/mongo/db/logical_session_cache_stats.idl
 */

#include "mongo/db/logical_session_cache_stats_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

//////db.serverStatus().logicalSessionRecordCache√¸¡Ó
constexpr StringData LogicalSessionCacheStats::kActiveSessionsCountFieldName;
constexpr StringData LogicalSessionCacheStats::kLastSessionsCollectionJobCursorsClosedFieldName;
constexpr StringData LogicalSessionCacheStats::kLastSessionsCollectionJobDurationMillisFieldName;
constexpr StringData LogicalSessionCacheStats::kLastSessionsCollectionJobEntriesEndedFieldName;
constexpr StringData LogicalSessionCacheStats::kLastSessionsCollectionJobEntriesRefreshedFieldName;
constexpr StringData LogicalSessionCacheStats::kLastSessionsCollectionJobTimestampFieldName;
constexpr StringData LogicalSessionCacheStats::kLastTransactionReaperJobDurationMillisFieldName;
constexpr StringData LogicalSessionCacheStats::kLastTransactionReaperJobEntriesCleanedUpFieldName;
constexpr StringData LogicalSessionCacheStats::kLastTransactionReaperJobTimestampFieldName;
constexpr StringData LogicalSessionCacheStats::kSessionsCollectionJobCountFieldName;
constexpr StringData LogicalSessionCacheStats::kTransactionReaperJobCountFieldName;


LogicalSessionCacheStats::LogicalSessionCacheStats() : _hasLastSessionsCollectionJobTimestamp(false), _hasLastTransactionReaperJobTimestamp(false) {
    // Used for initialization only
}

LogicalSessionCacheStats LogicalSessionCacheStats::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    LogicalSessionCacheStats object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void LogicalSessionCacheStats::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<11> usedFields;
    const size_t kActiveSessionsCountBit = 0;
    const size_t kSessionsCollectionJobCountBit = 1;
    const size_t kLastSessionsCollectionJobDurationMillisBit = 2;
    const size_t kLastSessionsCollectionJobTimestampBit = 3;
    const size_t kLastSessionsCollectionJobEntriesRefreshedBit = 4;
    const size_t kLastSessionsCollectionJobEntriesEndedBit = 5;
    const size_t kLastSessionsCollectionJobCursorsClosedBit = 6;
    const size_t kTransactionReaperJobCountBit = 7;
    const size_t kLastTransactionReaperJobDurationMillisBit = 8;
    const size_t kLastTransactionReaperJobTimestampBit = 9;
    const size_t kLastTransactionReaperJobEntriesCleanedUpBit = 10;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kActiveSessionsCountFieldName) {
            if (MONGO_unlikely(usedFields[kActiveSessionsCountBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kActiveSessionsCountBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _activeSessionsCount = element._numberInt();
            }
        }
        else if (fieldName == kSessionsCollectionJobCountFieldName) {
            if (MONGO_unlikely(usedFields[kSessionsCollectionJobCountBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSessionsCollectionJobCountBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _sessionsCollectionJobCount = element._numberInt();
            }
        }
        else if (fieldName == kLastSessionsCollectionJobDurationMillisFieldName) {
            if (MONGO_unlikely(usedFields[kLastSessionsCollectionJobDurationMillisBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastSessionsCollectionJobDurationMillisBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _lastSessionsCollectionJobDurationMillis = element._numberInt();
            }
        }
        else if (fieldName == kLastSessionsCollectionJobTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kLastSessionsCollectionJobTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastSessionsCollectionJobTimestampBit);

            _hasLastSessionsCollectionJobTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Date))) {
                _lastSessionsCollectionJobTimestamp = element.date();
            }
        }
        else if (fieldName == kLastSessionsCollectionJobEntriesRefreshedFieldName) {
            if (MONGO_unlikely(usedFields[kLastSessionsCollectionJobEntriesRefreshedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastSessionsCollectionJobEntriesRefreshedBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _lastSessionsCollectionJobEntriesRefreshed = element._numberInt();
            }
        }
        else if (fieldName == kLastSessionsCollectionJobEntriesEndedFieldName) {
            if (MONGO_unlikely(usedFields[kLastSessionsCollectionJobEntriesEndedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastSessionsCollectionJobEntriesEndedBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _lastSessionsCollectionJobEntriesEnded = element._numberInt();
            }
        }
        else if (fieldName == kLastSessionsCollectionJobCursorsClosedFieldName) {
            if (MONGO_unlikely(usedFields[kLastSessionsCollectionJobCursorsClosedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastSessionsCollectionJobCursorsClosedBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _lastSessionsCollectionJobCursorsClosed = element._numberInt();
            }
        }
        else if (fieldName == kTransactionReaperJobCountFieldName) {
            if (MONGO_unlikely(usedFields[kTransactionReaperJobCountBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTransactionReaperJobCountBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _transactionReaperJobCount = element._numberInt();
            }
        }
        else if (fieldName == kLastTransactionReaperJobDurationMillisFieldName) {
            if (MONGO_unlikely(usedFields[kLastTransactionReaperJobDurationMillisBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastTransactionReaperJobDurationMillisBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _lastTransactionReaperJobDurationMillis = element._numberInt();
            }
        }
        else if (fieldName == kLastTransactionReaperJobTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kLastTransactionReaperJobTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastTransactionReaperJobTimestampBit);

            _hasLastTransactionReaperJobTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Date))) {
                _lastTransactionReaperJobTimestamp = element.date();
            }
        }
        else if (fieldName == kLastTransactionReaperJobEntriesCleanedUpFieldName) {
            if (MONGO_unlikely(usedFields[kLastTransactionReaperJobEntriesCleanedUpBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastTransactionReaperJobEntriesCleanedUpBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _lastTransactionReaperJobEntriesCleanedUp = element._numberInt();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kActiveSessionsCountBit]) {
            _activeSessionsCount = 0;
        }
        if (!usedFields[kSessionsCollectionJobCountBit]) {
            _sessionsCollectionJobCount = 0;
        }
        if (!usedFields[kLastSessionsCollectionJobDurationMillisBit]) {
            _lastSessionsCollectionJobDurationMillis = 0;
        }
        if (!usedFields[kLastSessionsCollectionJobTimestampBit]) {
            ctxt.throwMissingField(kLastSessionsCollectionJobTimestampFieldName);
        }
        if (!usedFields[kLastSessionsCollectionJobEntriesRefreshedBit]) {
            _lastSessionsCollectionJobEntriesRefreshed = 0;
        }
        if (!usedFields[kLastSessionsCollectionJobEntriesEndedBit]) {
            _lastSessionsCollectionJobEntriesEnded = 0;
        }
        if (!usedFields[kLastSessionsCollectionJobCursorsClosedBit]) {
            _lastSessionsCollectionJobCursorsClosed = 0;
        }
        if (!usedFields[kTransactionReaperJobCountBit]) {
            _transactionReaperJobCount = 0;
        }
        if (!usedFields[kLastTransactionReaperJobDurationMillisBit]) {
            _lastTransactionReaperJobDurationMillis = 0;
        }
        if (!usedFields[kLastTransactionReaperJobTimestampBit]) {
            ctxt.throwMissingField(kLastTransactionReaperJobTimestampFieldName);
        }
        if (!usedFields[kLastTransactionReaperJobEntriesCleanedUpBit]) {
            _lastTransactionReaperJobEntriesCleanedUp = 0;
        }
    }

}

////db.serverStatus().logicalSessionRecordCache√¸¡Ó
void LogicalSessionCacheStats::serialize(BSONObjBuilder* builder) const {
    invariant(_hasLastSessionsCollectionJobTimestamp && _hasLastTransactionReaperJobTimestamp);

    builder->append(kActiveSessionsCountFieldName, _activeSessionsCount);

    builder->append(kSessionsCollectionJobCountFieldName, _sessionsCollectionJobCount);

    builder->append(kLastSessionsCollectionJobDurationMillisFieldName, _lastSessionsCollectionJobDurationMillis);

    builder->append(kLastSessionsCollectionJobTimestampFieldName, _lastSessionsCollectionJobTimestamp);

    builder->append(kLastSessionsCollectionJobEntriesRefreshedFieldName, _lastSessionsCollectionJobEntriesRefreshed);

    builder->append(kLastSessionsCollectionJobEntriesEndedFieldName, _lastSessionsCollectionJobEntriesEnded);

    builder->append(kLastSessionsCollectionJobCursorsClosedFieldName, _lastSessionsCollectionJobCursorsClosed);

    builder->append(kTransactionReaperJobCountFieldName, _transactionReaperJobCount);

    builder->append(kLastTransactionReaperJobDurationMillisFieldName, _lastTransactionReaperJobDurationMillis);

    builder->append(kLastTransactionReaperJobTimestampFieldName, _lastTransactionReaperJobTimestamp);

    builder->append(kLastTransactionReaperJobEntriesCleanedUpFieldName, _lastTransactionReaperJobEntriesCleanedUp);

}


BSONObj LogicalSessionCacheStats::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
