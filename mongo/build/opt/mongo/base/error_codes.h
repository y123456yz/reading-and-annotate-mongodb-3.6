/**
 *    Copyright 2017 MongoDB, Inc.
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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

#include "mongo/base/string_data.h"
#include "mongo/platform/compiler.h"

namespace mongo {

class Status;

enum class ErrorCategory {
    NetworkError,
    Interruption,
    NotMasterError,
    StaleShardingError,
    WriteConcernError,
    ShutdownError,
    ConnectionFatalMessageParseError,
    ExceededTimeLimitError,
};

/**
 * This is a generated class containing a table of error codes and their corresponding error
 * strings. The class is derived from the definitions in src/mongo/base/error_codes.err file and the
 * src/mongo/base/error_codes.tpl.h template.
 * //mongo/base/error_codes.err
 * Do not update this file directly. Update src/mongo/base/error_codes.err instead.
 */
class ErrorCodes {
public:
    // Explicitly 32-bits wide so that non-symbolic values,
    // like uassert codes, are valid.
    enum Error : std::int32_t {
        OK = 0,
        InternalError = 1,
        BadValue = 2,
        OBSOLETE_DuplicateKey = 3,
        NoSuchKey = 4,
        GraphContainsCycle = 5,
        HostUnreachable = 6,
        HostNotFound = 7,
        UnknownError = 8,
        FailedToParse = 9,
        CannotMutateObject = 10,
        UserNotFound = 11,
        UnsupportedFormat = 12,
        Unauthorized = 13,
        TypeMismatch = 14,
        Overflow = 15,
        InvalidLength = 16,
        ProtocolError = 17,
        AuthenticationFailed = 18,
        CannotReuseObject = 19,
        IllegalOperation = 20,
        EmptyArrayOperation = 21,
        InvalidBSON = 22,
        AlreadyInitialized = 23,
        LockTimeout = 24,
        RemoteValidationError = 25,
        NamespaceNotFound = 26,
        IndexNotFound = 27,
        PathNotViable = 28,
        NonExistentPath = 29,
        InvalidPath = 30,
        RoleNotFound = 31,
        RolesNotRelated = 32,
        PrivilegeNotFound = 33,
        CannotBackfillArray = 34,
        UserModificationFailed = 35,
        RemoteChangeDetected = 36,
        FileRenameFailed = 37,
        FileNotOpen = 38,
        FileStreamFailed = 39,
        ConflictingUpdateOperators = 40,
        FileAlreadyOpen = 41,
        LogWriteFailed = 42,
        CursorNotFound = 43,
        UserDataInconsistent = 45,
        LockBusy = 46,
        NoMatchingDocument = 47,
        NamespaceExists = 48,
        InvalidRoleModification = 49,
        ExceededTimeLimit = 50,
        ManualInterventionRequired = 51,
        DollarPrefixedFieldName = 52,
        InvalidIdField = 53,
        NotSingleValueField = 54,
        InvalidDBRef = 55,
        EmptyFieldName = 56,
        DottedFieldName = 57,
        RoleModificationFailed = 58,
        CommandNotFound = 59,
        OBSOLETE_DatabaseNotFound = 60,
        ShardKeyNotFound = 61,
        OplogOperationUnsupported = 62,
        StaleShardVersion = 63,
        WriteConcernFailed = 64,
        MultipleErrorsOccurred = 65,
        ImmutableField = 66,
        CannotCreateIndex = 67,
        IndexAlreadyExists = 68,
        AuthSchemaIncompatible = 69,
        ShardNotFound = 70,
        ReplicaSetNotFound = 71,
        InvalidOptions = 72,
        InvalidNamespace = 73,
        NodeNotFound = 74,
        WriteConcernLegacyOK = 75,
        NoReplicationEnabled = 76,
        OperationIncomplete = 77,
        CommandResultSchemaViolation = 78,
        UnknownReplWriteConcern = 79,
        RoleDataInconsistent = 80,
        NoMatchParseContext = 81,
        NoProgressMade = 82,
        RemoteResultsUnavailable = 83,
        DuplicateKeyValue = 84,
        IndexOptionsConflict = 85,
        IndexKeySpecsConflict = 86,
        CannotSplit = 87,
        SplitFailed_OBSOLETE = 88,
        NetworkTimeout = 89,
        CallbackCanceled = 90,
        ShutdownInProgress = 91,
        SecondaryAheadOfPrimary = 92,
        InvalidReplicaSetConfig = 93,
        NotYetInitialized = 94,
        NotSecondary = 95,
        OperationFailed = 96,
        NoProjectionFound = 97,
        DBPathInUse = 98,
        CannotSatisfyWriteConcern = 100,
        OutdatedClient = 101,
        IncompatibleAuditMetadata = 102,
        NewReplicaSetConfigurationIncompatible = 103,
        NodeNotElectable = 104,
        IncompatibleShardingMetadata = 105,
        DistributedClockSkewed = 106,
        LockFailed = 107,
        InconsistentReplicaSetNames = 108,
        ConfigurationInProgress = 109,
        CannotInitializeNodeWithData = 110,
        NotExactValueField = 111,
        WriteConflict = 112,
        InitialSyncFailure = 113,
        InitialSyncOplogSourceMissing = 114,
        CommandNotSupported = 115,
        DocTooLargeForCapped = 116,
        ConflictingOperationInProgress = 117,
        NamespaceNotSharded = 118,
        InvalidSyncSource = 119,
        OplogStartMissing = 120,
        DocumentValidationFailure = 121,
        OBSOLETE_ReadAfterOptimeTimeout = 122,
        NotAReplicaSet = 123,
        IncompatibleElectionProtocol = 124,
        CommandFailed = 125,
        RPCProtocolNegotiationFailed = 126,
        UnrecoverableRollbackError = 127,
        LockNotFound = 128,
        LockStateChangeFailed = 129,
        SymbolNotFound = 130,
        RLPInitializationFailed = 131,
        OBSOLETE_ConfigServersInconsistent = 132,
        FailedToSatisfyReadPreference = 133,
        ReadConcernMajorityNotAvailableYet = 134,
        StaleTerm = 135,
        CappedPositionLost = 136,
        IncompatibleShardingConfigVersion = 137,
        RemoteOplogStale = 138,
        JSInterpreterFailure = 139,
        InvalidSSLConfiguration = 140,
        SSLHandshakeFailed = 141,
        JSUncatchableError = 142,
        CursorInUse = 143,
        IncompatibleCatalogManager = 144,
        PooledConnectionsDropped = 145,
        ExceededMemoryLimit = 146,
        ZLibError = 147,
        ReadConcernMajorityNotEnabled = 148,
        NoConfigMaster = 149,
        StaleEpoch = 150,
        OperationCannotBeBatched = 151,
        OplogOutOfOrder = 152,
        ChunkTooBig = 153,
        InconsistentShardIdentity = 154,
        CannotApplyOplogWhilePrimary = 155,
        NeedsDocumentMove = 156,
        CanRepairToDowngrade = 157,
        MustUpgrade = 158,
        DurationOverflow = 159,
        MaxStalenessOutOfRange = 160,
        IncompatibleCollationVersion = 161,
        CollectionIsEmpty = 162,
        ZoneStillInUse = 163,
        InitialSyncActive = 164,
        ViewDepthLimitExceeded = 165,
        CommandNotSupportedOnView = 166,
        OptionNotSupportedOnView = 167,
        InvalidPipelineOperator = 168,
        CommandOnShardedViewNotSupportedOnMongod = 169,
        TooManyMatchingDocuments = 170,
        CannotIndexParallelArrays = 171,
        TransportSessionClosed = 172,
        TransportSessionNotFound = 173,
        TransportSessionUnknown = 174,
        QueryPlanKilled = 175,
        FileOpenFailed = 176,
        ZoneNotFound = 177,
        RangeOverlapConflict = 178,
        WindowsPdhError = 179,
        BadPerfCounterPath = 180,
        AmbiguousIndexKeyPattern = 181,
        InvalidViewDefinition = 182,
        ClientMetadataMissingField = 183,
        ClientMetadataAppNameTooLarge = 184,
        ClientMetadataDocumentTooLarge = 185,
        ClientMetadataCannotBeMutated = 186,
        LinearizableReadConcernError = 187,
        IncompatibleServerVersion = 188,
        PrimarySteppedDown = 189,
        MasterSlaveConnectionFailure = 190,
        OBSOLETE_BalancerLostDistributedLock = 191,
        FailPointEnabled = 192,
        NoShardingEnabled = 193,
        BalancerInterrupted = 194,
        ViewPipelineMaxSizeExceeded = 195,
        InvalidIndexSpecificationOption = 197,
        OBSOLETE_ReceivedOpReplyMessage = 198,
        ReplicaSetMonitorRemoved = 199,
        ChunkRangeCleanupPending = 200,
        CannotBuildIndexKeys = 201,
        NetworkInterfaceExceededTimeLimit = 202,
        ShardingStateNotInitialized = 203,
        TimeProofMismatch = 204,
        ClusterTimeFailsRateLimiter = 205,
        NoSuchSession = 206,
        InvalidUUID = 207,
        TooManyLocks = 208,
        StaleClusterTime = 209,
        CannotVerifyAndSignLogicalTime = 210,
        KeyNotFound = 211,
        IncompatibleRollbackAlgorithm = 212,
        DuplicateSession = 213,
        AuthenticationRestrictionUnmet = 214,
        DatabaseDropPending = 215,
        ElectionInProgress = 216,
        IncompleteTransactionHistory = 217,
        UpdateOperationFailed = 218,
        FTDCPathNotSet = 219,
        FTDCPathAlreadySet = 220,
        IndexModified = 221,
        CloseChangeStream = 222,
        IllegalOpMsgFlag = 223,
        QueryFeatureNotAllowed = 224,
        TransactionTooOld = 225,
        AtomicityFailure = 226,
        CannotImplicitlyCreateCollection = 227,
        SessionTransferIncomplete = 228,
        MustDowngrade = 229,
        DNSHostNotFound = 230,
        DNSProtocolError = 231,
        MaxSubPipelineDepthExceeded = 232,
        TooManyDocumentSequences = 233,
        RetryChangeStream = 234,
        SocketException = 9001,
        OBSOLETE_RecvStaleConfig = 9996,
        CannotGrowDocumentInCappedNamespace = 10003,
        NotMaster = 10107,
        DuplicateKey = 11000,
        InterruptedAtShutdown = 11600,
        Interrupted = 11601,
        InterruptedDueToReplStateChange = 11602,
        BackgroundOperationInProgressForDatabase = 12586,
        BackgroundOperationInProgressForNamespace = 12587,
        OBSOLETE_PrepareConfigsFailed = 13104,
        DatabaseDifferCase = 13297,
        ShardKeyTooBig = 13334,
        StaleConfig = 13388,
        NotMasterNoSlaveOk = 13435,
        NotMasterOrSecondary = 13436,
        OutOfDiskSpace = 14031,
        KeyTooLong = 17280,
        MaxError
    };

    static std::string errorString(Error err);

    /**
     * Parses an Error from its "name".  Returns UnknownError if "name" is unrecognized.
     *
     * NOTE: Also returns UnknownError for the string "UnknownError".
     */
    static Error fromString(StringData name);

    /**
     * Reuses a unique numeric code in a way that supresses the duplicate code detection. This
     * should only be used when testing error cases to ensure that the code under test fails in the
     * right place. It should NOT be used in non-test code to either make a new error site (use
     * ErrorCodes::Error(CODE) for that) or to see if a specific failure case occurred (use named
     * codes for that).
     */
    static Error duplicateCodeForTest(int code) {
        return static_cast<Error>(code);
    }

    /**
     * Generic predicate to test if a given error code is in a category.
     *
     * This version is intended to simplify forwarding by Status and DBException. Non-generic
     * callers should just use the specific isCategoryName() methods instead.
     */
    template <ErrorCategory category>
    static bool isA(Error code);

    static bool isNetworkError(Error code);
    static bool isInterruption(Error code);
    static bool isNotMasterError(Error code);
    static bool isStaleShardingError(Error code);
    static bool isWriteConcernError(Error code);
    static bool isShutdownError(Error code);
    static bool isConnectionFatalMessageParseError(Error code);
    static bool isExceededTimeLimitError(Error code);
};

std::ostream& operator<<(std::ostream& stream, ErrorCodes::Error code);

template <>
inline bool ErrorCodes::isA<ErrorCategory::NetworkError>(Error code) {
    return isNetworkError(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::Interruption>(Error code) {
    return isInterruption(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::NotMasterError>(Error code) {
    return isNotMasterError(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::StaleShardingError>(Error code) {
    return isStaleShardingError(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::WriteConcernError>(Error code) {
    return isWriteConcernError(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::ShutdownError>(Error code) {
    return isShutdownError(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::ConnectionFatalMessageParseError>(Error code) {
    return isConnectionFatalMessageParseError(code);
}
template <>
inline bool ErrorCodes::isA<ErrorCategory::ExceededTimeLimitError>(Error code) {
    return isExceededTimeLimitError(code);
}

/**
 * This namespace contains implementation details for our error handling code and should not be used
 * directly in general code.
 */
namespace error_details {

template <int32_t code>
constexpr bool isNamedCode = false;
template <>
constexpr bool isNamedCode<ErrorCodes::OK> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InternalError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::BadValue> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_DuplicateKey> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoSuchKey> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::GraphContainsCycle> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::HostUnreachable> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::HostNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UnknownError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FailedToParse> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotMutateObject> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UserNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UnsupportedFormat> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::Unauthorized> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TypeMismatch> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::Overflow> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidLength> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ProtocolError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::AuthenticationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotReuseObject> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IllegalOperation> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::EmptyArrayOperation> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidBSON> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::AlreadyInitialized> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LockTimeout> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RemoteValidationError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NamespaceNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IndexNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::PathNotViable> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NonExistentPath> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidPath> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RoleNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RolesNotRelated> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::PrivilegeNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotBackfillArray> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UserModificationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RemoteChangeDetected> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FileRenameFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FileNotOpen> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FileStreamFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ConflictingUpdateOperators> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FileAlreadyOpen> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LogWriteFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CursorNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UserDataInconsistent> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LockBusy> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoMatchingDocument> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NamespaceExists> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidRoleModification> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ExceededTimeLimit> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ManualInterventionRequired> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DollarPrefixedFieldName> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidIdField> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotSingleValueField> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidDBRef> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::EmptyFieldName> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DottedFieldName> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RoleModificationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CommandNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_DatabaseNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ShardKeyNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OplogOperationUnsupported> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::StaleShardVersion> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::WriteConcernFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::MultipleErrorsOccurred> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ImmutableField> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotCreateIndex> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IndexAlreadyExists> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::AuthSchemaIncompatible> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ShardNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ReplicaSetNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidOptions> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidNamespace> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NodeNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::WriteConcernLegacyOK> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoReplicationEnabled> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OperationIncomplete> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CommandResultSchemaViolation> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UnknownReplWriteConcern> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RoleDataInconsistent> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoMatchParseContext> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoProgressMade> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RemoteResultsUnavailable> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DuplicateKeyValue> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IndexOptionsConflict> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IndexKeySpecsConflict> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotSplit> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::SplitFailed_OBSOLETE> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NetworkTimeout> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CallbackCanceled> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ShutdownInProgress> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::SecondaryAheadOfPrimary> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidReplicaSetConfig> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotYetInitialized> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotSecondary> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OperationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoProjectionFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DBPathInUse> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotSatisfyWriteConcern> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OutdatedClient> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleAuditMetadata> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NewReplicaSetConfigurationIncompatible> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NodeNotElectable> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleShardingMetadata> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DistributedClockSkewed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LockFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InconsistentReplicaSetNames> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ConfigurationInProgress> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotInitializeNodeWithData> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotExactValueField> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::WriteConflict> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InitialSyncFailure> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InitialSyncOplogSourceMissing> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CommandNotSupported> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DocTooLargeForCapped> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ConflictingOperationInProgress> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NamespaceNotSharded> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidSyncSource> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OplogStartMissing> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DocumentValidationFailure> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_ReadAfterOptimeTimeout> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotAReplicaSet> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleElectionProtocol> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CommandFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RPCProtocolNegotiationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UnrecoverableRollbackError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LockNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LockStateChangeFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::SymbolNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RLPInitializationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_ConfigServersInconsistent> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FailedToSatisfyReadPreference> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ReadConcernMajorityNotAvailableYet> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::StaleTerm> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CappedPositionLost> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleShardingConfigVersion> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RemoteOplogStale> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::JSInterpreterFailure> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidSSLConfiguration> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::SSLHandshakeFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::JSUncatchableError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CursorInUse> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleCatalogManager> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::PooledConnectionsDropped> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ExceededMemoryLimit> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ZLibError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ReadConcernMajorityNotEnabled> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoConfigMaster> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::StaleEpoch> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OperationCannotBeBatched> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OplogOutOfOrder> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ChunkTooBig> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InconsistentShardIdentity> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotApplyOplogWhilePrimary> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NeedsDocumentMove> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CanRepairToDowngrade> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::MustUpgrade> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DurationOverflow> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::MaxStalenessOutOfRange> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleCollationVersion> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CollectionIsEmpty> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ZoneStillInUse> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InitialSyncActive> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ViewDepthLimitExceeded> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CommandNotSupportedOnView> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OptionNotSupportedOnView> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidPipelineOperator> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CommandOnShardedViewNotSupportedOnMongod> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TooManyMatchingDocuments> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotIndexParallelArrays> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TransportSessionClosed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TransportSessionNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TransportSessionUnknown> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::QueryPlanKilled> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FileOpenFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ZoneNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RangeOverlapConflict> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::WindowsPdhError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::BadPerfCounterPath> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::AmbiguousIndexKeyPattern> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidViewDefinition> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ClientMetadataMissingField> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ClientMetadataAppNameTooLarge> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ClientMetadataDocumentTooLarge> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ClientMetadataCannotBeMutated> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::LinearizableReadConcernError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleServerVersion> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::PrimarySteppedDown> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::MasterSlaveConnectionFailure> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_BalancerLostDistributedLock> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FailPointEnabled> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoShardingEnabled> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::BalancerInterrupted> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ViewPipelineMaxSizeExceeded> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidIndexSpecificationOption> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_ReceivedOpReplyMessage> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ReplicaSetMonitorRemoved> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ChunkRangeCleanupPending> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotBuildIndexKeys> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NetworkInterfaceExceededTimeLimit> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ShardingStateNotInitialized> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TimeProofMismatch> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ClusterTimeFailsRateLimiter> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NoSuchSession> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InvalidUUID> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TooManyLocks> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::StaleClusterTime> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotVerifyAndSignLogicalTime> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::KeyNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompatibleRollbackAlgorithm> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DuplicateSession> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::AuthenticationRestrictionUnmet> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DatabaseDropPending> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ElectionInProgress> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IncompleteTransactionHistory> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::UpdateOperationFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FTDCPathNotSet> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::FTDCPathAlreadySet> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IndexModified> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CloseChangeStream> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::IllegalOpMsgFlag> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::QueryFeatureNotAllowed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TransactionTooOld> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::AtomicityFailure> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotImplicitlyCreateCollection> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::SessionTransferIncomplete> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::MustDowngrade> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DNSHostNotFound> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DNSProtocolError> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::MaxSubPipelineDepthExceeded> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::TooManyDocumentSequences> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::RetryChangeStream> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::SocketException> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_RecvStaleConfig> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::CannotGrowDocumentInCappedNamespace> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotMaster> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DuplicateKey> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InterruptedAtShutdown> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::Interrupted> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::InterruptedDueToReplStateChange> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::BackgroundOperationInProgressForDatabase> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::BackgroundOperationInProgressForNamespace> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OBSOLETE_PrepareConfigsFailed> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::DatabaseDifferCase> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::ShardKeyTooBig> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::StaleConfig> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotMasterNoSlaveOk> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::NotMasterOrSecondary> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::OutOfDiskSpace> = true;
template <>
constexpr bool isNamedCode<ErrorCodes::KeyTooLong> = true;

MONGO_COMPILER_NORETURN void throwExceptionForStatus(const Status& status);

template <ErrorCategory... categories>
struct CategoryList;

template <ErrorCodes::Error code>
struct ErrorCategoriesForImpl {
    using type = CategoryList<>;
};

template <>
struct ErrorCategoriesForImpl<ErrorCodes::HostUnreachable> {
    using type = CategoryList<
        ErrorCategory::NetworkError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::HostNotFound> {
    using type = CategoryList<
        ErrorCategory::NetworkError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::ExceededTimeLimit> {
    using type = CategoryList<
        ErrorCategory::Interruption, 
        ErrorCategory::ExceededTimeLimitError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::StaleShardVersion> {
    using type = CategoryList<
        ErrorCategory::StaleShardingError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::WriteConcernFailed> {
    using type = CategoryList<
        ErrorCategory::WriteConcernError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::WriteConcernLegacyOK> {
    using type = CategoryList<
        ErrorCategory::WriteConcernError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::UnknownReplWriteConcern> {
    using type = CategoryList<
        ErrorCategory::WriteConcernError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::NetworkTimeout> {
    using type = CategoryList<
        ErrorCategory::NetworkError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::ShutdownInProgress> {
    using type = CategoryList<
        ErrorCategory::ShutdownError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::CannotSatisfyWriteConcern> {
    using type = CategoryList<
        ErrorCategory::WriteConcernError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::StaleEpoch> {
    using type = CategoryList<
        ErrorCategory::StaleShardingError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::PrimarySteppedDown> {
    using type = CategoryList<
        ErrorCategory::NotMasterError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::NetworkInterfaceExceededTimeLimit> {
    using type = CategoryList<
        ErrorCategory::ExceededTimeLimitError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::IllegalOpMsgFlag> {
    using type = CategoryList<
        ErrorCategory::ConnectionFatalMessageParseError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::TooManyDocumentSequences> {
    using type = CategoryList<
        ErrorCategory::ConnectionFatalMessageParseError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::SocketException> {
    using type = CategoryList<
        ErrorCategory::NetworkError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::NotMaster> {
    using type = CategoryList<
        ErrorCategory::NotMasterError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::InterruptedAtShutdown> {
    using type = CategoryList<
        ErrorCategory::Interruption, 
        ErrorCategory::ShutdownError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::Interrupted> {
    using type = CategoryList<
        ErrorCategory::Interruption
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::InterruptedDueToReplStateChange> {
    using type = CategoryList<
        ErrorCategory::Interruption, 
        ErrorCategory::NotMasterError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::StaleConfig> {
    using type = CategoryList<
        ErrorCategory::StaleShardingError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::NotMasterNoSlaveOk> {
    using type = CategoryList<
        ErrorCategory::NotMasterError
        >;
};
template <>
struct ErrorCategoriesForImpl<ErrorCodes::NotMasterOrSecondary> {
    using type = CategoryList<
        ErrorCategory::NotMasterError
        >;
};

template <ErrorCodes::Error code>
using ErrorCategoriesFor = typename ErrorCategoriesForImpl<code>::type;

}  // namespace error_details

}  // namespace mongo
