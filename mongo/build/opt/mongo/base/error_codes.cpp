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

#include "mongo/platform/basic.h"

#include "mongo/base/error_codes.h"

#include "mongo/base/static_assert.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

MONGO_STATIC_ASSERT(sizeof(ErrorCodes::Error) == sizeof(int));

std::string ErrorCodes::errorString(Error err) {
    switch (err) {
        case OK:
            return "OK";
        case InternalError:
            return "InternalError";
        case BadValue:
            return "BadValue";
        case OBSOLETE_DuplicateKey:
            return "OBSOLETE_DuplicateKey";
        case NoSuchKey:
            return "NoSuchKey";
        case GraphContainsCycle:
            return "GraphContainsCycle";
        case HostUnreachable:
            return "HostUnreachable";
        case HostNotFound:
            return "HostNotFound";
        case UnknownError:
            return "UnknownError";
        case FailedToParse:
            return "FailedToParse";
        case CannotMutateObject:
            return "CannotMutateObject";
        case UserNotFound:
            return "UserNotFound";
        case UnsupportedFormat:
            return "UnsupportedFormat";
        case Unauthorized:
            return "Unauthorized";
        case TypeMismatch:
            return "TypeMismatch";
        case Overflow:
            return "Overflow";
        case InvalidLength:
            return "InvalidLength";
        case ProtocolError:
            return "ProtocolError";
        case AuthenticationFailed:
            return "AuthenticationFailed";
        case CannotReuseObject:
            return "CannotReuseObject";
        case IllegalOperation:
            return "IllegalOperation";
        case EmptyArrayOperation:
            return "EmptyArrayOperation";
        case InvalidBSON:
            return "InvalidBSON";
        case AlreadyInitialized:
            return "AlreadyInitialized";
        case LockTimeout:
            return "LockTimeout";
        case RemoteValidationError:
            return "RemoteValidationError";
        case NamespaceNotFound:
            return "NamespaceNotFound";
        case IndexNotFound:
            return "IndexNotFound";
        case PathNotViable:
            return "PathNotViable";
        case NonExistentPath:
            return "NonExistentPath";
        case InvalidPath:
            return "InvalidPath";
        case RoleNotFound:
            return "RoleNotFound";
        case RolesNotRelated:
            return "RolesNotRelated";
        case PrivilegeNotFound:
            return "PrivilegeNotFound";
        case CannotBackfillArray:
            return "CannotBackfillArray";
        case UserModificationFailed:
            return "UserModificationFailed";
        case RemoteChangeDetected:
            return "RemoteChangeDetected";
        case FileRenameFailed:
            return "FileRenameFailed";
        case FileNotOpen:
            return "FileNotOpen";
        case FileStreamFailed:
            return "FileStreamFailed";
        case ConflictingUpdateOperators:
            return "ConflictingUpdateOperators";
        case FileAlreadyOpen:
            return "FileAlreadyOpen";
        case LogWriteFailed:
            return "LogWriteFailed";
        case CursorNotFound:
            return "CursorNotFound";
        case UserDataInconsistent:
            return "UserDataInconsistent";
        case LockBusy:
            return "LockBusy";
        case NoMatchingDocument:
            return "NoMatchingDocument";
        case NamespaceExists:
            return "NamespaceExists";
        case InvalidRoleModification:
            return "InvalidRoleModification";
        case ExceededTimeLimit:
            return "ExceededTimeLimit";
        case ManualInterventionRequired:
            return "ManualInterventionRequired";
        case DollarPrefixedFieldName:
            return "DollarPrefixedFieldName";
        case InvalidIdField:
            return "InvalidIdField";
        case NotSingleValueField:
            return "NotSingleValueField";
        case InvalidDBRef:
            return "InvalidDBRef";
        case EmptyFieldName:
            return "EmptyFieldName";
        case DottedFieldName:
            return "DottedFieldName";
        case RoleModificationFailed:
            return "RoleModificationFailed";
        case CommandNotFound:
            return "CommandNotFound";
        case OBSOLETE_DatabaseNotFound:
            return "OBSOLETE_DatabaseNotFound";
        case ShardKeyNotFound:
            return "ShardKeyNotFound";
        case OplogOperationUnsupported:
            return "OplogOperationUnsupported";
        case StaleShardVersion:
            return "StaleShardVersion";
        case WriteConcernFailed:
            return "WriteConcernFailed";
        case MultipleErrorsOccurred:
            return "MultipleErrorsOccurred";
        case ImmutableField:
            return "ImmutableField";
        case CannotCreateIndex:
            return "CannotCreateIndex";
        case IndexAlreadyExists:
            return "IndexAlreadyExists";
        case AuthSchemaIncompatible:
            return "AuthSchemaIncompatible";
        case ShardNotFound:
            return "ShardNotFound";
        case ReplicaSetNotFound:
            return "ReplicaSetNotFound";
        case InvalidOptions:
            return "InvalidOptions";
        case InvalidNamespace:
            return "InvalidNamespace";
        case NodeNotFound:
            return "NodeNotFound";
        case WriteConcernLegacyOK:
            return "WriteConcernLegacyOK";
        case NoReplicationEnabled:
            return "NoReplicationEnabled";
        case OperationIncomplete:
            return "OperationIncomplete";
        case CommandResultSchemaViolation:
            return "CommandResultSchemaViolation";
        case UnknownReplWriteConcern:
            return "UnknownReplWriteConcern";
        case RoleDataInconsistent:
            return "RoleDataInconsistent";
        case NoMatchParseContext:
            return "NoMatchParseContext";
        case NoProgressMade:
            return "NoProgressMade";
        case RemoteResultsUnavailable:
            return "RemoteResultsUnavailable";
        case DuplicateKeyValue:
            return "DuplicateKeyValue";
        case IndexOptionsConflict:
            return "IndexOptionsConflict";
        case IndexKeySpecsConflict:
            return "IndexKeySpecsConflict";
        case CannotSplit:
            return "CannotSplit";
        case SplitFailed_OBSOLETE:
            return "SplitFailed_OBSOLETE";
        case NetworkTimeout:
            return "NetworkTimeout";
        case CallbackCanceled:
            return "CallbackCanceled";
        case ShutdownInProgress:
            return "ShutdownInProgress";
        case SecondaryAheadOfPrimary:
            return "SecondaryAheadOfPrimary";
        case InvalidReplicaSetConfig:
            return "InvalidReplicaSetConfig";
        case NotYetInitialized:
            return "NotYetInitialized";
        case NotSecondary:
            return "NotSecondary";
        case OperationFailed:
            return "OperationFailed";
        case NoProjectionFound:
            return "NoProjectionFound";
        case DBPathInUse:
            return "DBPathInUse";
        case CannotSatisfyWriteConcern:
            return "CannotSatisfyWriteConcern";
        case OutdatedClient:
            return "OutdatedClient";
        case IncompatibleAuditMetadata:
            return "IncompatibleAuditMetadata";
        case NewReplicaSetConfigurationIncompatible:
            return "NewReplicaSetConfigurationIncompatible";
        case NodeNotElectable:
            return "NodeNotElectable";
        case IncompatibleShardingMetadata:
            return "IncompatibleShardingMetadata";
        case DistributedClockSkewed:
            return "DistributedClockSkewed";
        case LockFailed:
            return "LockFailed";
        case InconsistentReplicaSetNames:
            return "InconsistentReplicaSetNames";
        case ConfigurationInProgress:
            return "ConfigurationInProgress";
        case CannotInitializeNodeWithData:
            return "CannotInitializeNodeWithData";
        case NotExactValueField:
            return "NotExactValueField";
        case WriteConflict:
            return "WriteConflict";
        case InitialSyncFailure:
            return "InitialSyncFailure";
        case InitialSyncOplogSourceMissing:
            return "InitialSyncOplogSourceMissing";
        case CommandNotSupported:
            return "CommandNotSupported";
        case DocTooLargeForCapped:
            return "DocTooLargeForCapped";
        case ConflictingOperationInProgress:
            return "ConflictingOperationInProgress";
        case NamespaceNotSharded:
            return "NamespaceNotSharded";
        case InvalidSyncSource:
            return "InvalidSyncSource";
        case OplogStartMissing:
            return "OplogStartMissing";
        case DocumentValidationFailure:
            return "DocumentValidationFailure";
        case OBSOLETE_ReadAfterOptimeTimeout:
            return "OBSOLETE_ReadAfterOptimeTimeout";
        case NotAReplicaSet:
            return "NotAReplicaSet";
        case IncompatibleElectionProtocol:
            return "IncompatibleElectionProtocol";
        case CommandFailed:
            return "CommandFailed";
        case RPCProtocolNegotiationFailed:
            return "RPCProtocolNegotiationFailed";
        case UnrecoverableRollbackError:
            return "UnrecoverableRollbackError";
        case LockNotFound:
            return "LockNotFound";
        case LockStateChangeFailed:
            return "LockStateChangeFailed";
        case SymbolNotFound:
            return "SymbolNotFound";
        case RLPInitializationFailed:
            return "RLPInitializationFailed";
        case OBSOLETE_ConfigServersInconsistent:
            return "OBSOLETE_ConfigServersInconsistent";
        case FailedToSatisfyReadPreference:
            return "FailedToSatisfyReadPreference";
        case ReadConcernMajorityNotAvailableYet:
            return "ReadConcernMajorityNotAvailableYet";
        case StaleTerm:
            return "StaleTerm";
        case CappedPositionLost:
            return "CappedPositionLost";
        case IncompatibleShardingConfigVersion:
            return "IncompatibleShardingConfigVersion";
        case RemoteOplogStale:
            return "RemoteOplogStale";
        case JSInterpreterFailure:
            return "JSInterpreterFailure";
        case InvalidSSLConfiguration:
            return "InvalidSSLConfiguration";
        case SSLHandshakeFailed:
            return "SSLHandshakeFailed";
        case JSUncatchableError:
            return "JSUncatchableError";
        case CursorInUse:
            return "CursorInUse";
        case IncompatibleCatalogManager:
            return "IncompatibleCatalogManager";
        case PooledConnectionsDropped:
            return "PooledConnectionsDropped";
        case ExceededMemoryLimit:
            return "ExceededMemoryLimit";
        case ZLibError:
            return "ZLibError";
        case ReadConcernMajorityNotEnabled:
            return "ReadConcernMajorityNotEnabled";
        case NoConfigMaster:
            return "NoConfigMaster";
        case StaleEpoch:
            return "StaleEpoch";
        case OperationCannotBeBatched:
            return "OperationCannotBeBatched";
        case OplogOutOfOrder:
            return "OplogOutOfOrder";
        case ChunkTooBig:
            return "ChunkTooBig";
        case InconsistentShardIdentity:
            return "InconsistentShardIdentity";
        case CannotApplyOplogWhilePrimary:
            return "CannotApplyOplogWhilePrimary";
        case NeedsDocumentMove:
            return "NeedsDocumentMove";
        case CanRepairToDowngrade:
            return "CanRepairToDowngrade";
        case MustUpgrade:
            return "MustUpgrade";
        case DurationOverflow:
            return "DurationOverflow";
        case MaxStalenessOutOfRange:
            return "MaxStalenessOutOfRange";
        case IncompatibleCollationVersion:
            return "IncompatibleCollationVersion";
        case CollectionIsEmpty:
            return "CollectionIsEmpty";
        case ZoneStillInUse:
            return "ZoneStillInUse";
        case InitialSyncActive:
            return "InitialSyncActive";
        case ViewDepthLimitExceeded:
            return "ViewDepthLimitExceeded";
        case CommandNotSupportedOnView:
            return "CommandNotSupportedOnView";
        case OptionNotSupportedOnView:
            return "OptionNotSupportedOnView";
        case InvalidPipelineOperator:
            return "InvalidPipelineOperator";
        case CommandOnShardedViewNotSupportedOnMongod:
            return "CommandOnShardedViewNotSupportedOnMongod";
        case TooManyMatchingDocuments:
            return "TooManyMatchingDocuments";
        case CannotIndexParallelArrays:
            return "CannotIndexParallelArrays";
        case TransportSessionClosed:
            return "TransportSessionClosed";
        case TransportSessionNotFound:
            return "TransportSessionNotFound";
        case TransportSessionUnknown:
            return "TransportSessionUnknown";
        case QueryPlanKilled:
            return "QueryPlanKilled";
        case FileOpenFailed:
            return "FileOpenFailed";
        case ZoneNotFound:
            return "ZoneNotFound";
        case RangeOverlapConflict:
            return "RangeOverlapConflict";
        case WindowsPdhError:
            return "WindowsPdhError";
        case BadPerfCounterPath:
            return "BadPerfCounterPath";
        case AmbiguousIndexKeyPattern:
            return "AmbiguousIndexKeyPattern";
        case InvalidViewDefinition:
            return "InvalidViewDefinition";
        case ClientMetadataMissingField:
            return "ClientMetadataMissingField";
        case ClientMetadataAppNameTooLarge:
            return "ClientMetadataAppNameTooLarge";
        case ClientMetadataDocumentTooLarge:
            return "ClientMetadataDocumentTooLarge";
        case ClientMetadataCannotBeMutated:
            return "ClientMetadataCannotBeMutated";
        case LinearizableReadConcernError:
            return "LinearizableReadConcernError";
        case IncompatibleServerVersion:
            return "IncompatibleServerVersion";
        case PrimarySteppedDown:
            return "PrimarySteppedDown";
        case MasterSlaveConnectionFailure:
            return "MasterSlaveConnectionFailure";
        case OBSOLETE_BalancerLostDistributedLock:
            return "OBSOLETE_BalancerLostDistributedLock";
        case FailPointEnabled:
            return "FailPointEnabled";
        case NoShardingEnabled:
            return "NoShardingEnabled";
        case BalancerInterrupted:
            return "BalancerInterrupted";
        case ViewPipelineMaxSizeExceeded:
            return "ViewPipelineMaxSizeExceeded";
        case InvalidIndexSpecificationOption:
            return "InvalidIndexSpecificationOption";
        case OBSOLETE_ReceivedOpReplyMessage:
            return "OBSOLETE_ReceivedOpReplyMessage";
        case ReplicaSetMonitorRemoved:
            return "ReplicaSetMonitorRemoved";
        case ChunkRangeCleanupPending:
            return "ChunkRangeCleanupPending";
        case CannotBuildIndexKeys:
            return "CannotBuildIndexKeys";
        case NetworkInterfaceExceededTimeLimit:
            return "NetworkInterfaceExceededTimeLimit";
        case ShardingStateNotInitialized:
            return "ShardingStateNotInitialized";
        case TimeProofMismatch:
            return "TimeProofMismatch";
        case ClusterTimeFailsRateLimiter:
            return "ClusterTimeFailsRateLimiter";
        case NoSuchSession:
            return "NoSuchSession";
        case InvalidUUID:
            return "InvalidUUID";
        case TooManyLocks:
            return "TooManyLocks";
        case StaleClusterTime:
            return "StaleClusterTime";
        case CannotVerifyAndSignLogicalTime:
            return "CannotVerifyAndSignLogicalTime";
        case KeyNotFound:
            return "KeyNotFound";
        case IncompatibleRollbackAlgorithm:
            return "IncompatibleRollbackAlgorithm";
        case DuplicateSession:
            return "DuplicateSession";
        case AuthenticationRestrictionUnmet:
            return "AuthenticationRestrictionUnmet";
        case DatabaseDropPending:
            return "DatabaseDropPending";
        case ElectionInProgress:
            return "ElectionInProgress";
        case IncompleteTransactionHistory:
            return "IncompleteTransactionHistory";
        case UpdateOperationFailed:
            return "UpdateOperationFailed";
        case FTDCPathNotSet:
            return "FTDCPathNotSet";
        case FTDCPathAlreadySet:
            return "FTDCPathAlreadySet";
        case IndexModified:
            return "IndexModified";
        case CloseChangeStream:
            return "CloseChangeStream";
        case IllegalOpMsgFlag:
            return "IllegalOpMsgFlag";
        case QueryFeatureNotAllowed:
            return "QueryFeatureNotAllowed";
        case TransactionTooOld:
            return "TransactionTooOld";
        case AtomicityFailure:
            return "AtomicityFailure";
        case CannotImplicitlyCreateCollection:
            return "CannotImplicitlyCreateCollection";
        case SessionTransferIncomplete:
            return "SessionTransferIncomplete";
        case MustDowngrade:
            return "MustDowngrade";
        case DNSHostNotFound:
            return "DNSHostNotFound";
        case DNSProtocolError:
            return "DNSProtocolError";
        case MaxSubPipelineDepthExceeded:
            return "MaxSubPipelineDepthExceeded";
        case TooManyDocumentSequences:
            return "TooManyDocumentSequences";
        case RetryChangeStream:
            return "RetryChangeStream";
        case SocketException:
            return "SocketException";
        case OBSOLETE_RecvStaleConfig:
            return "OBSOLETE_RecvStaleConfig";
        case CannotGrowDocumentInCappedNamespace:
            return "CannotGrowDocumentInCappedNamespace";
        case NotMaster:
            return "NotMaster";
        case DuplicateKey:
            return "DuplicateKey";
        case InterruptedAtShutdown:
            return "InterruptedAtShutdown";
        case Interrupted:
            return "Interrupted";
        case InterruptedDueToReplStateChange:
            return "InterruptedDueToReplStateChange";
        case BackgroundOperationInProgressForDatabase:
            return "BackgroundOperationInProgressForDatabase";
        case BackgroundOperationInProgressForNamespace:
            return "BackgroundOperationInProgressForNamespace";
        case OBSOLETE_PrepareConfigsFailed:
            return "OBSOLETE_PrepareConfigsFailed";
        case DatabaseDifferCase:
            return "DatabaseDifferCase";
        case ShardKeyTooBig:
            return "ShardKeyTooBig";
        case StaleConfig:
            return "StaleConfig";
        case NotMasterNoSlaveOk:
            return "NotMasterNoSlaveOk";
        case NotMasterOrSecondary:
            return "NotMasterOrSecondary";
        case OutOfDiskSpace:
            return "OutOfDiskSpace";
        case KeyTooLong:
            return "KeyTooLong";
        default:
            return mongoutils::str::stream() << "Location" << int(err);
    }
}

ErrorCodes::Error ErrorCodes::fromString(StringData name) {
    if (name == "OK"_sd)
        return OK;
    if (name == "InternalError"_sd)
        return InternalError;
    if (name == "BadValue"_sd)
        return BadValue;
    if (name == "OBSOLETE_DuplicateKey"_sd)
        return OBSOLETE_DuplicateKey;
    if (name == "NoSuchKey"_sd)
        return NoSuchKey;
    if (name == "GraphContainsCycle"_sd)
        return GraphContainsCycle;
    if (name == "HostUnreachable"_sd)
        return HostUnreachable;
    if (name == "HostNotFound"_sd)
        return HostNotFound;
    if (name == "UnknownError"_sd)
        return UnknownError;
    if (name == "FailedToParse"_sd)
        return FailedToParse;
    if (name == "CannotMutateObject"_sd)
        return CannotMutateObject;
    if (name == "UserNotFound"_sd)
        return UserNotFound;
    if (name == "UnsupportedFormat"_sd)
        return UnsupportedFormat;
    if (name == "Unauthorized"_sd)
        return Unauthorized;
    if (name == "TypeMismatch"_sd)
        return TypeMismatch;
    if (name == "Overflow"_sd)
        return Overflow;
    if (name == "InvalidLength"_sd)
        return InvalidLength;
    if (name == "ProtocolError"_sd)
        return ProtocolError;
    if (name == "AuthenticationFailed"_sd)
        return AuthenticationFailed;
    if (name == "CannotReuseObject"_sd)
        return CannotReuseObject;
    if (name == "IllegalOperation"_sd)
        return IllegalOperation;
    if (name == "EmptyArrayOperation"_sd)
        return EmptyArrayOperation;
    if (name == "InvalidBSON"_sd)
        return InvalidBSON;
    if (name == "AlreadyInitialized"_sd)
        return AlreadyInitialized;
    if (name == "LockTimeout"_sd)
        return LockTimeout;
    if (name == "RemoteValidationError"_sd)
        return RemoteValidationError;
    if (name == "NamespaceNotFound"_sd)
        return NamespaceNotFound;
    if (name == "IndexNotFound"_sd)
        return IndexNotFound;
    if (name == "PathNotViable"_sd)
        return PathNotViable;
    if (name == "NonExistentPath"_sd)
        return NonExistentPath;
    if (name == "InvalidPath"_sd)
        return InvalidPath;
    if (name == "RoleNotFound"_sd)
        return RoleNotFound;
    if (name == "RolesNotRelated"_sd)
        return RolesNotRelated;
    if (name == "PrivilegeNotFound"_sd)
        return PrivilegeNotFound;
    if (name == "CannotBackfillArray"_sd)
        return CannotBackfillArray;
    if (name == "UserModificationFailed"_sd)
        return UserModificationFailed;
    if (name == "RemoteChangeDetected"_sd)
        return RemoteChangeDetected;
    if (name == "FileRenameFailed"_sd)
        return FileRenameFailed;
    if (name == "FileNotOpen"_sd)
        return FileNotOpen;
    if (name == "FileStreamFailed"_sd)
        return FileStreamFailed;
    if (name == "ConflictingUpdateOperators"_sd)
        return ConflictingUpdateOperators;
    if (name == "FileAlreadyOpen"_sd)
        return FileAlreadyOpen;
    if (name == "LogWriteFailed"_sd)
        return LogWriteFailed;
    if (name == "CursorNotFound"_sd)
        return CursorNotFound;
    if (name == "UserDataInconsistent"_sd)
        return UserDataInconsistent;
    if (name == "LockBusy"_sd)
        return LockBusy;
    if (name == "NoMatchingDocument"_sd)
        return NoMatchingDocument;
    if (name == "NamespaceExists"_sd)
        return NamespaceExists;
    if (name == "InvalidRoleModification"_sd)
        return InvalidRoleModification;
    if (name == "ExceededTimeLimit"_sd)
        return ExceededTimeLimit;
    if (name == "ManualInterventionRequired"_sd)
        return ManualInterventionRequired;
    if (name == "DollarPrefixedFieldName"_sd)
        return DollarPrefixedFieldName;
    if (name == "InvalidIdField"_sd)
        return InvalidIdField;
    if (name == "NotSingleValueField"_sd)
        return NotSingleValueField;
    if (name == "InvalidDBRef"_sd)
        return InvalidDBRef;
    if (name == "EmptyFieldName"_sd)
        return EmptyFieldName;
    if (name == "DottedFieldName"_sd)
        return DottedFieldName;
    if (name == "RoleModificationFailed"_sd)
        return RoleModificationFailed;
    if (name == "CommandNotFound"_sd)
        return CommandNotFound;
    if (name == "OBSOLETE_DatabaseNotFound"_sd)
        return OBSOLETE_DatabaseNotFound;
    if (name == "ShardKeyNotFound"_sd)
        return ShardKeyNotFound;
    if (name == "OplogOperationUnsupported"_sd)
        return OplogOperationUnsupported;
    if (name == "StaleShardVersion"_sd)
        return StaleShardVersion;
    if (name == "WriteConcernFailed"_sd)
        return WriteConcernFailed;
    if (name == "MultipleErrorsOccurred"_sd)
        return MultipleErrorsOccurred;
    if (name == "ImmutableField"_sd)
        return ImmutableField;
    if (name == "CannotCreateIndex"_sd)
        return CannotCreateIndex;
    if (name == "IndexAlreadyExists"_sd)
        return IndexAlreadyExists;
    if (name == "AuthSchemaIncompatible"_sd)
        return AuthSchemaIncompatible;
    if (name == "ShardNotFound"_sd)
        return ShardNotFound;
    if (name == "ReplicaSetNotFound"_sd)
        return ReplicaSetNotFound;
    if (name == "InvalidOptions"_sd)
        return InvalidOptions;
    if (name == "InvalidNamespace"_sd)
        return InvalidNamespace;
    if (name == "NodeNotFound"_sd)
        return NodeNotFound;
    if (name == "WriteConcernLegacyOK"_sd)
        return WriteConcernLegacyOK;
    if (name == "NoReplicationEnabled"_sd)
        return NoReplicationEnabled;
    if (name == "OperationIncomplete"_sd)
        return OperationIncomplete;
    if (name == "CommandResultSchemaViolation"_sd)
        return CommandResultSchemaViolation;
    if (name == "UnknownReplWriteConcern"_sd)
        return UnknownReplWriteConcern;
    if (name == "RoleDataInconsistent"_sd)
        return RoleDataInconsistent;
    if (name == "NoMatchParseContext"_sd)
        return NoMatchParseContext;
    if (name == "NoProgressMade"_sd)
        return NoProgressMade;
    if (name == "RemoteResultsUnavailable"_sd)
        return RemoteResultsUnavailable;
    if (name == "DuplicateKeyValue"_sd)
        return DuplicateKeyValue;
    if (name == "IndexOptionsConflict"_sd)
        return IndexOptionsConflict;
    if (name == "IndexKeySpecsConflict"_sd)
        return IndexKeySpecsConflict;
    if (name == "CannotSplit"_sd)
        return CannotSplit;
    if (name == "SplitFailed_OBSOLETE"_sd)
        return SplitFailed_OBSOLETE;
    if (name == "NetworkTimeout"_sd)
        return NetworkTimeout;
    if (name == "CallbackCanceled"_sd)
        return CallbackCanceled;
    if (name == "ShutdownInProgress"_sd)
        return ShutdownInProgress;
    if (name == "SecondaryAheadOfPrimary"_sd)
        return SecondaryAheadOfPrimary;
    if (name == "InvalidReplicaSetConfig"_sd)
        return InvalidReplicaSetConfig;
    if (name == "NotYetInitialized"_sd)
        return NotYetInitialized;
    if (name == "NotSecondary"_sd)
        return NotSecondary;
    if (name == "OperationFailed"_sd)
        return OperationFailed;
    if (name == "NoProjectionFound"_sd)
        return NoProjectionFound;
    if (name == "DBPathInUse"_sd)
        return DBPathInUse;
    if (name == "CannotSatisfyWriteConcern"_sd)
        return CannotSatisfyWriteConcern;
    if (name == "OutdatedClient"_sd)
        return OutdatedClient;
    if (name == "IncompatibleAuditMetadata"_sd)
        return IncompatibleAuditMetadata;
    if (name == "NewReplicaSetConfigurationIncompatible"_sd)
        return NewReplicaSetConfigurationIncompatible;
    if (name == "NodeNotElectable"_sd)
        return NodeNotElectable;
    if (name == "IncompatibleShardingMetadata"_sd)
        return IncompatibleShardingMetadata;
    if (name == "DistributedClockSkewed"_sd)
        return DistributedClockSkewed;
    if (name == "LockFailed"_sd)
        return LockFailed;
    if (name == "InconsistentReplicaSetNames"_sd)
        return InconsistentReplicaSetNames;
    if (name == "ConfigurationInProgress"_sd)
        return ConfigurationInProgress;
    if (name == "CannotInitializeNodeWithData"_sd)
        return CannotInitializeNodeWithData;
    if (name == "NotExactValueField"_sd)
        return NotExactValueField;
    if (name == "WriteConflict"_sd)
        return WriteConflict;
    if (name == "InitialSyncFailure"_sd)
        return InitialSyncFailure;
    if (name == "InitialSyncOplogSourceMissing"_sd)
        return InitialSyncOplogSourceMissing;
    if (name == "CommandNotSupported"_sd)
        return CommandNotSupported;
    if (name == "DocTooLargeForCapped"_sd)
        return DocTooLargeForCapped;
    if (name == "ConflictingOperationInProgress"_sd)
        return ConflictingOperationInProgress;
    if (name == "NamespaceNotSharded"_sd)
        return NamespaceNotSharded;
    if (name == "InvalidSyncSource"_sd)
        return InvalidSyncSource;
    if (name == "OplogStartMissing"_sd)
        return OplogStartMissing;
    if (name == "DocumentValidationFailure"_sd)
        return DocumentValidationFailure;
    if (name == "OBSOLETE_ReadAfterOptimeTimeout"_sd)
        return OBSOLETE_ReadAfterOptimeTimeout;
    if (name == "NotAReplicaSet"_sd)
        return NotAReplicaSet;
    if (name == "IncompatibleElectionProtocol"_sd)
        return IncompatibleElectionProtocol;
    if (name == "CommandFailed"_sd)
        return CommandFailed;
    if (name == "RPCProtocolNegotiationFailed"_sd)
        return RPCProtocolNegotiationFailed;
    if (name == "UnrecoverableRollbackError"_sd)
        return UnrecoverableRollbackError;
    if (name == "LockNotFound"_sd)
        return LockNotFound;
    if (name == "LockStateChangeFailed"_sd)
        return LockStateChangeFailed;
    if (name == "SymbolNotFound"_sd)
        return SymbolNotFound;
    if (name == "RLPInitializationFailed"_sd)
        return RLPInitializationFailed;
    if (name == "OBSOLETE_ConfigServersInconsistent"_sd)
        return OBSOLETE_ConfigServersInconsistent;
    if (name == "FailedToSatisfyReadPreference"_sd)
        return FailedToSatisfyReadPreference;
    if (name == "ReadConcernMajorityNotAvailableYet"_sd)
        return ReadConcernMajorityNotAvailableYet;
    if (name == "StaleTerm"_sd)
        return StaleTerm;
    if (name == "CappedPositionLost"_sd)
        return CappedPositionLost;
    if (name == "IncompatibleShardingConfigVersion"_sd)
        return IncompatibleShardingConfigVersion;
    if (name == "RemoteOplogStale"_sd)
        return RemoteOplogStale;
    if (name == "JSInterpreterFailure"_sd)
        return JSInterpreterFailure;
    if (name == "InvalidSSLConfiguration"_sd)
        return InvalidSSLConfiguration;
    if (name == "SSLHandshakeFailed"_sd)
        return SSLHandshakeFailed;
    if (name == "JSUncatchableError"_sd)
        return JSUncatchableError;
    if (name == "CursorInUse"_sd)
        return CursorInUse;
    if (name == "IncompatibleCatalogManager"_sd)
        return IncompatibleCatalogManager;
    if (name == "PooledConnectionsDropped"_sd)
        return PooledConnectionsDropped;
    if (name == "ExceededMemoryLimit"_sd)
        return ExceededMemoryLimit;
    if (name == "ZLibError"_sd)
        return ZLibError;
    if (name == "ReadConcernMajorityNotEnabled"_sd)
        return ReadConcernMajorityNotEnabled;
    if (name == "NoConfigMaster"_sd)
        return NoConfigMaster;
    if (name == "StaleEpoch"_sd)
        return StaleEpoch;
    if (name == "OperationCannotBeBatched"_sd)
        return OperationCannotBeBatched;
    if (name == "OplogOutOfOrder"_sd)
        return OplogOutOfOrder;
    if (name == "ChunkTooBig"_sd)
        return ChunkTooBig;
    if (name == "InconsistentShardIdentity"_sd)
        return InconsistentShardIdentity;
    if (name == "CannotApplyOplogWhilePrimary"_sd)
        return CannotApplyOplogWhilePrimary;
    if (name == "NeedsDocumentMove"_sd)
        return NeedsDocumentMove;
    if (name == "CanRepairToDowngrade"_sd)
        return CanRepairToDowngrade;
    if (name == "MustUpgrade"_sd)
        return MustUpgrade;
    if (name == "DurationOverflow"_sd)
        return DurationOverflow;
    if (name == "MaxStalenessOutOfRange"_sd)
        return MaxStalenessOutOfRange;
    if (name == "IncompatibleCollationVersion"_sd)
        return IncompatibleCollationVersion;
    if (name == "CollectionIsEmpty"_sd)
        return CollectionIsEmpty;
    if (name == "ZoneStillInUse"_sd)
        return ZoneStillInUse;
    if (name == "InitialSyncActive"_sd)
        return InitialSyncActive;
    if (name == "ViewDepthLimitExceeded"_sd)
        return ViewDepthLimitExceeded;
    if (name == "CommandNotSupportedOnView"_sd)
        return CommandNotSupportedOnView;
    if (name == "OptionNotSupportedOnView"_sd)
        return OptionNotSupportedOnView;
    if (name == "InvalidPipelineOperator"_sd)
        return InvalidPipelineOperator;
    if (name == "CommandOnShardedViewNotSupportedOnMongod"_sd)
        return CommandOnShardedViewNotSupportedOnMongod;
    if (name == "TooManyMatchingDocuments"_sd)
        return TooManyMatchingDocuments;
    if (name == "CannotIndexParallelArrays"_sd)
        return CannotIndexParallelArrays;
    if (name == "TransportSessionClosed"_sd)
        return TransportSessionClosed;
    if (name == "TransportSessionNotFound"_sd)
        return TransportSessionNotFound;
    if (name == "TransportSessionUnknown"_sd)
        return TransportSessionUnknown;
    if (name == "QueryPlanKilled"_sd)
        return QueryPlanKilled;
    if (name == "FileOpenFailed"_sd)
        return FileOpenFailed;
    if (name == "ZoneNotFound"_sd)
        return ZoneNotFound;
    if (name == "RangeOverlapConflict"_sd)
        return RangeOverlapConflict;
    if (name == "WindowsPdhError"_sd)
        return WindowsPdhError;
    if (name == "BadPerfCounterPath"_sd)
        return BadPerfCounterPath;
    if (name == "AmbiguousIndexKeyPattern"_sd)
        return AmbiguousIndexKeyPattern;
    if (name == "InvalidViewDefinition"_sd)
        return InvalidViewDefinition;
    if (name == "ClientMetadataMissingField"_sd)
        return ClientMetadataMissingField;
    if (name == "ClientMetadataAppNameTooLarge"_sd)
        return ClientMetadataAppNameTooLarge;
    if (name == "ClientMetadataDocumentTooLarge"_sd)
        return ClientMetadataDocumentTooLarge;
    if (name == "ClientMetadataCannotBeMutated"_sd)
        return ClientMetadataCannotBeMutated;
    if (name == "LinearizableReadConcernError"_sd)
        return LinearizableReadConcernError;
    if (name == "IncompatibleServerVersion"_sd)
        return IncompatibleServerVersion;
    if (name == "PrimarySteppedDown"_sd)
        return PrimarySteppedDown;
    if (name == "MasterSlaveConnectionFailure"_sd)
        return MasterSlaveConnectionFailure;
    if (name == "OBSOLETE_BalancerLostDistributedLock"_sd)
        return OBSOLETE_BalancerLostDistributedLock;
    if (name == "FailPointEnabled"_sd)
        return FailPointEnabled;
    if (name == "NoShardingEnabled"_sd)
        return NoShardingEnabled;
    if (name == "BalancerInterrupted"_sd)
        return BalancerInterrupted;
    if (name == "ViewPipelineMaxSizeExceeded"_sd)
        return ViewPipelineMaxSizeExceeded;
    if (name == "InvalidIndexSpecificationOption"_sd)
        return InvalidIndexSpecificationOption;
    if (name == "OBSOLETE_ReceivedOpReplyMessage"_sd)
        return OBSOLETE_ReceivedOpReplyMessage;
    if (name == "ReplicaSetMonitorRemoved"_sd)
        return ReplicaSetMonitorRemoved;
    if (name == "ChunkRangeCleanupPending"_sd)
        return ChunkRangeCleanupPending;
    if (name == "CannotBuildIndexKeys"_sd)
        return CannotBuildIndexKeys;
    if (name == "NetworkInterfaceExceededTimeLimit"_sd)
        return NetworkInterfaceExceededTimeLimit;
    if (name == "ShardingStateNotInitialized"_sd)
        return ShardingStateNotInitialized;
    if (name == "TimeProofMismatch"_sd)
        return TimeProofMismatch;
    if (name == "ClusterTimeFailsRateLimiter"_sd)
        return ClusterTimeFailsRateLimiter;
    if (name == "NoSuchSession"_sd)
        return NoSuchSession;
    if (name == "InvalidUUID"_sd)
        return InvalidUUID;
    if (name == "TooManyLocks"_sd)
        return TooManyLocks;
    if (name == "StaleClusterTime"_sd)
        return StaleClusterTime;
    if (name == "CannotVerifyAndSignLogicalTime"_sd)
        return CannotVerifyAndSignLogicalTime;
    if (name == "KeyNotFound"_sd)
        return KeyNotFound;
    if (name == "IncompatibleRollbackAlgorithm"_sd)
        return IncompatibleRollbackAlgorithm;
    if (name == "DuplicateSession"_sd)
        return DuplicateSession;
    if (name == "AuthenticationRestrictionUnmet"_sd)
        return AuthenticationRestrictionUnmet;
    if (name == "DatabaseDropPending"_sd)
        return DatabaseDropPending;
    if (name == "ElectionInProgress"_sd)
        return ElectionInProgress;
    if (name == "IncompleteTransactionHistory"_sd)
        return IncompleteTransactionHistory;
    if (name == "UpdateOperationFailed"_sd)
        return UpdateOperationFailed;
    if (name == "FTDCPathNotSet"_sd)
        return FTDCPathNotSet;
    if (name == "FTDCPathAlreadySet"_sd)
        return FTDCPathAlreadySet;
    if (name == "IndexModified"_sd)
        return IndexModified;
    if (name == "CloseChangeStream"_sd)
        return CloseChangeStream;
    if (name == "IllegalOpMsgFlag"_sd)
        return IllegalOpMsgFlag;
    if (name == "QueryFeatureNotAllowed"_sd)
        return QueryFeatureNotAllowed;
    if (name == "TransactionTooOld"_sd)
        return TransactionTooOld;
    if (name == "AtomicityFailure"_sd)
        return AtomicityFailure;
    if (name == "CannotImplicitlyCreateCollection"_sd)
        return CannotImplicitlyCreateCollection;
    if (name == "SessionTransferIncomplete"_sd)
        return SessionTransferIncomplete;
    if (name == "MustDowngrade"_sd)
        return MustDowngrade;
    if (name == "DNSHostNotFound"_sd)
        return DNSHostNotFound;
    if (name == "DNSProtocolError"_sd)
        return DNSProtocolError;
    if (name == "MaxSubPipelineDepthExceeded"_sd)
        return MaxSubPipelineDepthExceeded;
    if (name == "TooManyDocumentSequences"_sd)
        return TooManyDocumentSequences;
    if (name == "RetryChangeStream"_sd)
        return RetryChangeStream;
    if (name == "SocketException"_sd)
        return SocketException;
    if (name == "OBSOLETE_RecvStaleConfig"_sd)
        return OBSOLETE_RecvStaleConfig;
    if (name == "CannotGrowDocumentInCappedNamespace"_sd)
        return CannotGrowDocumentInCappedNamespace;
    if (name == "NotMaster"_sd)
        return NotMaster;
    if (name == "DuplicateKey"_sd)
        return DuplicateKey;
    if (name == "InterruptedAtShutdown"_sd)
        return InterruptedAtShutdown;
    if (name == "Interrupted"_sd)
        return Interrupted;
    if (name == "InterruptedDueToReplStateChange"_sd)
        return InterruptedDueToReplStateChange;
    if (name == "BackgroundOperationInProgressForDatabase"_sd)
        return BackgroundOperationInProgressForDatabase;
    if (name == "BackgroundOperationInProgressForNamespace"_sd)
        return BackgroundOperationInProgressForNamespace;
    if (name == "OBSOLETE_PrepareConfigsFailed"_sd)
        return OBSOLETE_PrepareConfigsFailed;
    if (name == "DatabaseDifferCase"_sd)
        return DatabaseDifferCase;
    if (name == "ShardKeyTooBig"_sd)
        return ShardKeyTooBig;
    if (name == "StaleConfig"_sd)
        return StaleConfig;
    if (name == "NotMasterNoSlaveOk"_sd)
        return NotMasterNoSlaveOk;
    if (name == "NotMasterOrSecondary"_sd)
        return NotMasterOrSecondary;
    if (name == "OutOfDiskSpace"_sd)
        return OutOfDiskSpace;
    if (name == "KeyTooLong"_sd)
        return KeyTooLong;
    return UnknownError;
}

std::ostream& operator<<(std::ostream& stream, ErrorCodes::Error code) {
    return stream << ErrorCodes::errorString(code);
}

bool ErrorCodes::isNetworkError(Error err) {
    switch (err) {
        case HostUnreachable:
            return true;
        case HostNotFound:
            return true;
        case NetworkTimeout:
            return true;
        case SocketException:
            return true;
        default:
            return false;
    }
}
bool ErrorCodes::isInterruption(Error err) {
    switch (err) {
        case Interrupted:
            return true;
        case InterruptedAtShutdown:
            return true;
        case InterruptedDueToReplStateChange:
            return true;
        case ExceededTimeLimit:
            return true;
        default:
            return false;
    }
}
bool ErrorCodes::isNotMasterError(Error err) {
    switch (err) {
        case NotMaster:
            return true;
        case NotMasterNoSlaveOk:
            return true;
        case NotMasterOrSecondary:
            return true;
        case InterruptedDueToReplStateChange:
            return true;
        case PrimarySteppedDown:
            return true;
        default:
            return false;
    }
}

//代理转发版本信息问题用这里判断
bool ErrorCodes::isStaleShardingError(Error err) {
    switch (err) {
		//StaleConfigException中生成
        case StaleConfig:
            return true;
		//serializeReply  CollectionMetadata::checkChunkIsValid  BatchWriteExec::executeBatch
        case StaleShardVersion:
            return true;
		//CursorResponse::parseFromBSON  checkMetadataForSuccessfulSplitChunk
        case StaleEpoch:
            return true;
        default:
            return false;
    }
}
bool ErrorCodes::isWriteConcernError(Error err) {
    switch (err) {
        case WriteConcernFailed:
            return true;
        case WriteConcernLegacyOK:
            return true;
        case UnknownReplWriteConcern:
            return true;
        case CannotSatisfyWriteConcern:
            return true;
        default:
            return false;
    }
}
bool ErrorCodes::isShutdownError(Error err) {
    switch (err) {
        case ShutdownInProgress:
            return true;
        case InterruptedAtShutdown:
            return true;
        default:
            return false;
    }
}
bool ErrorCodes::isConnectionFatalMessageParseError(Error err) {
    switch (err) {
        case IllegalOpMsgFlag:
            return true;
        case TooManyDocumentSequences:
            return true;
        default:
            return false;
    }
}
bool ErrorCodes::isExceededTimeLimitError(Error err) {
    switch (err) {
        case ExceededTimeLimit:
            return true;
        case NetworkInterfaceExceededTimeLimit:
            return true;
        default:
            return false;
    }
}

void error_details::throwExceptionForStatus(const Status& status) {
    /**
     * This type is used for all exceptions that don't have a more specific type. It is defined
     * locally in this function to prevent anyone from catching it specifically separately from
     * AssertionException.
     */
    class NonspecificAssertionException final : public AssertionException {
    public:
        using AssertionException::AssertionException;

    private:
        void defineOnlyInFinalSubclassToPreventSlicing() final {}
    };

    switch (status.code()) {
        case ErrorCodes::OK:
            throw ExceptionFor<ErrorCodes::OK>(status);
        case ErrorCodes::InternalError:
            throw ExceptionFor<ErrorCodes::InternalError>(status);
        case ErrorCodes::BadValue:
            throw ExceptionFor<ErrorCodes::BadValue>(status);
        case ErrorCodes::OBSOLETE_DuplicateKey:
            throw ExceptionFor<ErrorCodes::OBSOLETE_DuplicateKey>(status);
        case ErrorCodes::NoSuchKey:
            throw ExceptionFor<ErrorCodes::NoSuchKey>(status);
        case ErrorCodes::GraphContainsCycle:
            throw ExceptionFor<ErrorCodes::GraphContainsCycle>(status);
        case ErrorCodes::HostUnreachable:
            throw ExceptionFor<ErrorCodes::HostUnreachable>(status);
        case ErrorCodes::HostNotFound:
            throw ExceptionFor<ErrorCodes::HostNotFound>(status);
        case ErrorCodes::UnknownError:
            throw ExceptionFor<ErrorCodes::UnknownError>(status);
        case ErrorCodes::FailedToParse:
            throw ExceptionFor<ErrorCodes::FailedToParse>(status);
        case ErrorCodes::CannotMutateObject:
            throw ExceptionFor<ErrorCodes::CannotMutateObject>(status);
        case ErrorCodes::UserNotFound:
            throw ExceptionFor<ErrorCodes::UserNotFound>(status);
        case ErrorCodes::UnsupportedFormat:
            throw ExceptionFor<ErrorCodes::UnsupportedFormat>(status);
        case ErrorCodes::Unauthorized:
            throw ExceptionFor<ErrorCodes::Unauthorized>(status);
        case ErrorCodes::TypeMismatch:
            throw ExceptionFor<ErrorCodes::TypeMismatch>(status);
        case ErrorCodes::Overflow:
            throw ExceptionFor<ErrorCodes::Overflow>(status);
        case ErrorCodes::InvalidLength:
            throw ExceptionFor<ErrorCodes::InvalidLength>(status);
        case ErrorCodes::ProtocolError:
            throw ExceptionFor<ErrorCodes::ProtocolError>(status);
        case ErrorCodes::AuthenticationFailed:
            throw ExceptionFor<ErrorCodes::AuthenticationFailed>(status);
        case ErrorCodes::CannotReuseObject:
            throw ExceptionFor<ErrorCodes::CannotReuseObject>(status);
        case ErrorCodes::IllegalOperation:
            throw ExceptionFor<ErrorCodes::IllegalOperation>(status);
        case ErrorCodes::EmptyArrayOperation:
            throw ExceptionFor<ErrorCodes::EmptyArrayOperation>(status);
        case ErrorCodes::InvalidBSON:
            throw ExceptionFor<ErrorCodes::InvalidBSON>(status);
        case ErrorCodes::AlreadyInitialized:
            throw ExceptionFor<ErrorCodes::AlreadyInitialized>(status);
        case ErrorCodes::LockTimeout:
            throw ExceptionFor<ErrorCodes::LockTimeout>(status);
        case ErrorCodes::RemoteValidationError:
            throw ExceptionFor<ErrorCodes::RemoteValidationError>(status);
        case ErrorCodes::NamespaceNotFound:
            throw ExceptionFor<ErrorCodes::NamespaceNotFound>(status);
        case ErrorCodes::IndexNotFound:
            throw ExceptionFor<ErrorCodes::IndexNotFound>(status);
        case ErrorCodes::PathNotViable:
            throw ExceptionFor<ErrorCodes::PathNotViable>(status);
        case ErrorCodes::NonExistentPath:
            throw ExceptionFor<ErrorCodes::NonExistentPath>(status);
        case ErrorCodes::InvalidPath:
            throw ExceptionFor<ErrorCodes::InvalidPath>(status);
        case ErrorCodes::RoleNotFound:
            throw ExceptionFor<ErrorCodes::RoleNotFound>(status);
        case ErrorCodes::RolesNotRelated:
            throw ExceptionFor<ErrorCodes::RolesNotRelated>(status);
        case ErrorCodes::PrivilegeNotFound:
            throw ExceptionFor<ErrorCodes::PrivilegeNotFound>(status);
        case ErrorCodes::CannotBackfillArray:
            throw ExceptionFor<ErrorCodes::CannotBackfillArray>(status);
        case ErrorCodes::UserModificationFailed:
            throw ExceptionFor<ErrorCodes::UserModificationFailed>(status);
        case ErrorCodes::RemoteChangeDetected:
            throw ExceptionFor<ErrorCodes::RemoteChangeDetected>(status);
        case ErrorCodes::FileRenameFailed:
            throw ExceptionFor<ErrorCodes::FileRenameFailed>(status);
        case ErrorCodes::FileNotOpen:
            throw ExceptionFor<ErrorCodes::FileNotOpen>(status);
        case ErrorCodes::FileStreamFailed:
            throw ExceptionFor<ErrorCodes::FileStreamFailed>(status);
        case ErrorCodes::ConflictingUpdateOperators:
            throw ExceptionFor<ErrorCodes::ConflictingUpdateOperators>(status);
        case ErrorCodes::FileAlreadyOpen:
            throw ExceptionFor<ErrorCodes::FileAlreadyOpen>(status);
        case ErrorCodes::LogWriteFailed:
            throw ExceptionFor<ErrorCodes::LogWriteFailed>(status);
        case ErrorCodes::CursorNotFound:
            throw ExceptionFor<ErrorCodes::CursorNotFound>(status);
        case ErrorCodes::UserDataInconsistent:
            throw ExceptionFor<ErrorCodes::UserDataInconsistent>(status);
        case ErrorCodes::LockBusy:
            throw ExceptionFor<ErrorCodes::LockBusy>(status);
        case ErrorCodes::NoMatchingDocument:
            throw ExceptionFor<ErrorCodes::NoMatchingDocument>(status);
        case ErrorCodes::NamespaceExists:
            throw ExceptionFor<ErrorCodes::NamespaceExists>(status);
        case ErrorCodes::InvalidRoleModification:
            throw ExceptionFor<ErrorCodes::InvalidRoleModification>(status);
        case ErrorCodes::ExceededTimeLimit:
            throw ExceptionFor<ErrorCodes::ExceededTimeLimit>(status);
        case ErrorCodes::ManualInterventionRequired:
            throw ExceptionFor<ErrorCodes::ManualInterventionRequired>(status);
        case ErrorCodes::DollarPrefixedFieldName:
            throw ExceptionFor<ErrorCodes::DollarPrefixedFieldName>(status);
        case ErrorCodes::InvalidIdField:
            throw ExceptionFor<ErrorCodes::InvalidIdField>(status);
        case ErrorCodes::NotSingleValueField:
            throw ExceptionFor<ErrorCodes::NotSingleValueField>(status);
        case ErrorCodes::InvalidDBRef:
            throw ExceptionFor<ErrorCodes::InvalidDBRef>(status);
        case ErrorCodes::EmptyFieldName:
            throw ExceptionFor<ErrorCodes::EmptyFieldName>(status);
        case ErrorCodes::DottedFieldName:
            throw ExceptionFor<ErrorCodes::DottedFieldName>(status);
        case ErrorCodes::RoleModificationFailed:
            throw ExceptionFor<ErrorCodes::RoleModificationFailed>(status);
        case ErrorCodes::CommandNotFound:
            throw ExceptionFor<ErrorCodes::CommandNotFound>(status);
        case ErrorCodes::OBSOLETE_DatabaseNotFound:
            throw ExceptionFor<ErrorCodes::OBSOLETE_DatabaseNotFound>(status);
        case ErrorCodes::ShardKeyNotFound:
            throw ExceptionFor<ErrorCodes::ShardKeyNotFound>(status);
        case ErrorCodes::OplogOperationUnsupported:
            throw ExceptionFor<ErrorCodes::OplogOperationUnsupported>(status);
        case ErrorCodes::StaleShardVersion:
            throw ExceptionFor<ErrorCodes::StaleShardVersion>(status);
        case ErrorCodes::WriteConcernFailed:
            throw ExceptionFor<ErrorCodes::WriteConcernFailed>(status);
        case ErrorCodes::MultipleErrorsOccurred:
            throw ExceptionFor<ErrorCodes::MultipleErrorsOccurred>(status);
        case ErrorCodes::ImmutableField:
            throw ExceptionFor<ErrorCodes::ImmutableField>(status);
        case ErrorCodes::CannotCreateIndex:
            throw ExceptionFor<ErrorCodes::CannotCreateIndex>(status);
        case ErrorCodes::IndexAlreadyExists:
            throw ExceptionFor<ErrorCodes::IndexAlreadyExists>(status);
        case ErrorCodes::AuthSchemaIncompatible:
            throw ExceptionFor<ErrorCodes::AuthSchemaIncompatible>(status);
        case ErrorCodes::ShardNotFound:
            throw ExceptionFor<ErrorCodes::ShardNotFound>(status);
        case ErrorCodes::ReplicaSetNotFound:
            throw ExceptionFor<ErrorCodes::ReplicaSetNotFound>(status);
        case ErrorCodes::InvalidOptions:
            throw ExceptionFor<ErrorCodes::InvalidOptions>(status);
        case ErrorCodes::InvalidNamespace:
            throw ExceptionFor<ErrorCodes::InvalidNamespace>(status);
        case ErrorCodes::NodeNotFound:
            throw ExceptionFor<ErrorCodes::NodeNotFound>(status);
        case ErrorCodes::WriteConcernLegacyOK:
            throw ExceptionFor<ErrorCodes::WriteConcernLegacyOK>(status);
        case ErrorCodes::NoReplicationEnabled:
            throw ExceptionFor<ErrorCodes::NoReplicationEnabled>(status);
        case ErrorCodes::OperationIncomplete:
            throw ExceptionFor<ErrorCodes::OperationIncomplete>(status);
        case ErrorCodes::CommandResultSchemaViolation:
            throw ExceptionFor<ErrorCodes::CommandResultSchemaViolation>(status);
        case ErrorCodes::UnknownReplWriteConcern:
            throw ExceptionFor<ErrorCodes::UnknownReplWriteConcern>(status);
        case ErrorCodes::RoleDataInconsistent:
            throw ExceptionFor<ErrorCodes::RoleDataInconsistent>(status);
        case ErrorCodes::NoMatchParseContext:
            throw ExceptionFor<ErrorCodes::NoMatchParseContext>(status);
        case ErrorCodes::NoProgressMade:
            throw ExceptionFor<ErrorCodes::NoProgressMade>(status);
        case ErrorCodes::RemoteResultsUnavailable:
            throw ExceptionFor<ErrorCodes::RemoteResultsUnavailable>(status);
        case ErrorCodes::DuplicateKeyValue:
            throw ExceptionFor<ErrorCodes::DuplicateKeyValue>(status);
        case ErrorCodes::IndexOptionsConflict:
            throw ExceptionFor<ErrorCodes::IndexOptionsConflict>(status);
        case ErrorCodes::IndexKeySpecsConflict:
            throw ExceptionFor<ErrorCodes::IndexKeySpecsConflict>(status);
        case ErrorCodes::CannotSplit:
            throw ExceptionFor<ErrorCodes::CannotSplit>(status);
        case ErrorCodes::SplitFailed_OBSOLETE:
            throw ExceptionFor<ErrorCodes::SplitFailed_OBSOLETE>(status);
        case ErrorCodes::NetworkTimeout:
            throw ExceptionFor<ErrorCodes::NetworkTimeout>(status);
        case ErrorCodes::CallbackCanceled:
            throw ExceptionFor<ErrorCodes::CallbackCanceled>(status);
        case ErrorCodes::ShutdownInProgress:
            throw ExceptionFor<ErrorCodes::ShutdownInProgress>(status);
        case ErrorCodes::SecondaryAheadOfPrimary:
            throw ExceptionFor<ErrorCodes::SecondaryAheadOfPrimary>(status);
        case ErrorCodes::InvalidReplicaSetConfig:
            throw ExceptionFor<ErrorCodes::InvalidReplicaSetConfig>(status);
        case ErrorCodes::NotYetInitialized:
            throw ExceptionFor<ErrorCodes::NotYetInitialized>(status);
        case ErrorCodes::NotSecondary:
            throw ExceptionFor<ErrorCodes::NotSecondary>(status);
        case ErrorCodes::OperationFailed:
            throw ExceptionFor<ErrorCodes::OperationFailed>(status);
        case ErrorCodes::NoProjectionFound:
            throw ExceptionFor<ErrorCodes::NoProjectionFound>(status);
        case ErrorCodes::DBPathInUse:
            throw ExceptionFor<ErrorCodes::DBPathInUse>(status);
        case ErrorCodes::CannotSatisfyWriteConcern:
            throw ExceptionFor<ErrorCodes::CannotSatisfyWriteConcern>(status);
        case ErrorCodes::OutdatedClient:
            throw ExceptionFor<ErrorCodes::OutdatedClient>(status);
        case ErrorCodes::IncompatibleAuditMetadata:
            throw ExceptionFor<ErrorCodes::IncompatibleAuditMetadata>(status);
        case ErrorCodes::NewReplicaSetConfigurationIncompatible:
            throw ExceptionFor<ErrorCodes::NewReplicaSetConfigurationIncompatible>(status);
        case ErrorCodes::NodeNotElectable:
            throw ExceptionFor<ErrorCodes::NodeNotElectable>(status);
        case ErrorCodes::IncompatibleShardingMetadata:
            throw ExceptionFor<ErrorCodes::IncompatibleShardingMetadata>(status);
        case ErrorCodes::DistributedClockSkewed:
            throw ExceptionFor<ErrorCodes::DistributedClockSkewed>(status);
        case ErrorCodes::LockFailed:
            throw ExceptionFor<ErrorCodes::LockFailed>(status);
        case ErrorCodes::InconsistentReplicaSetNames:
            throw ExceptionFor<ErrorCodes::InconsistentReplicaSetNames>(status);
        case ErrorCodes::ConfigurationInProgress:
            throw ExceptionFor<ErrorCodes::ConfigurationInProgress>(status);
        case ErrorCodes::CannotInitializeNodeWithData:
            throw ExceptionFor<ErrorCodes::CannotInitializeNodeWithData>(status);
        case ErrorCodes::NotExactValueField:
            throw ExceptionFor<ErrorCodes::NotExactValueField>(status);
        case ErrorCodes::WriteConflict:
            throw ExceptionFor<ErrorCodes::WriteConflict>(status);
        case ErrorCodes::InitialSyncFailure:
            throw ExceptionFor<ErrorCodes::InitialSyncFailure>(status);
        case ErrorCodes::InitialSyncOplogSourceMissing:
            throw ExceptionFor<ErrorCodes::InitialSyncOplogSourceMissing>(status);
        case ErrorCodes::CommandNotSupported:
            throw ExceptionFor<ErrorCodes::CommandNotSupported>(status);
        case ErrorCodes::DocTooLargeForCapped:
            throw ExceptionFor<ErrorCodes::DocTooLargeForCapped>(status);
        case ErrorCodes::ConflictingOperationInProgress:
            throw ExceptionFor<ErrorCodes::ConflictingOperationInProgress>(status);
        case ErrorCodes::NamespaceNotSharded:
            throw ExceptionFor<ErrorCodes::NamespaceNotSharded>(status);
        case ErrorCodes::InvalidSyncSource:
            throw ExceptionFor<ErrorCodes::InvalidSyncSource>(status);
        case ErrorCodes::OplogStartMissing:
            throw ExceptionFor<ErrorCodes::OplogStartMissing>(status);
        case ErrorCodes::DocumentValidationFailure:
            throw ExceptionFor<ErrorCodes::DocumentValidationFailure>(status);
        case ErrorCodes::OBSOLETE_ReadAfterOptimeTimeout:
            throw ExceptionFor<ErrorCodes::OBSOLETE_ReadAfterOptimeTimeout>(status);
        case ErrorCodes::NotAReplicaSet:
            throw ExceptionFor<ErrorCodes::NotAReplicaSet>(status);
        case ErrorCodes::IncompatibleElectionProtocol:
            throw ExceptionFor<ErrorCodes::IncompatibleElectionProtocol>(status);
        case ErrorCodes::CommandFailed:
            throw ExceptionFor<ErrorCodes::CommandFailed>(status);
        case ErrorCodes::RPCProtocolNegotiationFailed:
            throw ExceptionFor<ErrorCodes::RPCProtocolNegotiationFailed>(status);
        case ErrorCodes::UnrecoverableRollbackError:
            throw ExceptionFor<ErrorCodes::UnrecoverableRollbackError>(status);
        case ErrorCodes::LockNotFound:
            throw ExceptionFor<ErrorCodes::LockNotFound>(status);
        case ErrorCodes::LockStateChangeFailed:
            throw ExceptionFor<ErrorCodes::LockStateChangeFailed>(status);
        case ErrorCodes::SymbolNotFound:
            throw ExceptionFor<ErrorCodes::SymbolNotFound>(status);
        case ErrorCodes::RLPInitializationFailed:
            throw ExceptionFor<ErrorCodes::RLPInitializationFailed>(status);
        case ErrorCodes::OBSOLETE_ConfigServersInconsistent:
            throw ExceptionFor<ErrorCodes::OBSOLETE_ConfigServersInconsistent>(status);
        case ErrorCodes::FailedToSatisfyReadPreference:
            throw ExceptionFor<ErrorCodes::FailedToSatisfyReadPreference>(status);
        case ErrorCodes::ReadConcernMajorityNotAvailableYet:
            throw ExceptionFor<ErrorCodes::ReadConcernMajorityNotAvailableYet>(status);
        case ErrorCodes::StaleTerm:
            throw ExceptionFor<ErrorCodes::StaleTerm>(status);
        case ErrorCodes::CappedPositionLost:
            throw ExceptionFor<ErrorCodes::CappedPositionLost>(status);
        case ErrorCodes::IncompatibleShardingConfigVersion:
            throw ExceptionFor<ErrorCodes::IncompatibleShardingConfigVersion>(status);
        case ErrorCodes::RemoteOplogStale:
            throw ExceptionFor<ErrorCodes::RemoteOplogStale>(status);
        case ErrorCodes::JSInterpreterFailure:
            throw ExceptionFor<ErrorCodes::JSInterpreterFailure>(status);
        case ErrorCodes::InvalidSSLConfiguration:
            throw ExceptionFor<ErrorCodes::InvalidSSLConfiguration>(status);
        case ErrorCodes::SSLHandshakeFailed:
            throw ExceptionFor<ErrorCodes::SSLHandshakeFailed>(status);
        case ErrorCodes::JSUncatchableError:
            throw ExceptionFor<ErrorCodes::JSUncatchableError>(status);
        case ErrorCodes::CursorInUse:
            throw ExceptionFor<ErrorCodes::CursorInUse>(status);
        case ErrorCodes::IncompatibleCatalogManager:
            throw ExceptionFor<ErrorCodes::IncompatibleCatalogManager>(status);
        case ErrorCodes::PooledConnectionsDropped:
            throw ExceptionFor<ErrorCodes::PooledConnectionsDropped>(status);
        case ErrorCodes::ExceededMemoryLimit:
            throw ExceptionFor<ErrorCodes::ExceededMemoryLimit>(status);
        case ErrorCodes::ZLibError:
            throw ExceptionFor<ErrorCodes::ZLibError>(status);
        case ErrorCodes::ReadConcernMajorityNotEnabled:
            throw ExceptionFor<ErrorCodes::ReadConcernMajorityNotEnabled>(status);
        case ErrorCodes::NoConfigMaster:
            throw ExceptionFor<ErrorCodes::NoConfigMaster>(status);
        case ErrorCodes::StaleEpoch:
            throw ExceptionFor<ErrorCodes::StaleEpoch>(status);
        case ErrorCodes::OperationCannotBeBatched:
            throw ExceptionFor<ErrorCodes::OperationCannotBeBatched>(status);
        case ErrorCodes::OplogOutOfOrder:
            throw ExceptionFor<ErrorCodes::OplogOutOfOrder>(status);
        case ErrorCodes::ChunkTooBig:
            throw ExceptionFor<ErrorCodes::ChunkTooBig>(status);
        case ErrorCodes::InconsistentShardIdentity:
            throw ExceptionFor<ErrorCodes::InconsistentShardIdentity>(status);
        case ErrorCodes::CannotApplyOplogWhilePrimary:
            throw ExceptionFor<ErrorCodes::CannotApplyOplogWhilePrimary>(status);
        case ErrorCodes::NeedsDocumentMove:
            throw ExceptionFor<ErrorCodes::NeedsDocumentMove>(status);
        case ErrorCodes::CanRepairToDowngrade:
            throw ExceptionFor<ErrorCodes::CanRepairToDowngrade>(status);
        case ErrorCodes::MustUpgrade:
            throw ExceptionFor<ErrorCodes::MustUpgrade>(status);
        case ErrorCodes::DurationOverflow:
            throw ExceptionFor<ErrorCodes::DurationOverflow>(status);
        case ErrorCodes::MaxStalenessOutOfRange:
            throw ExceptionFor<ErrorCodes::MaxStalenessOutOfRange>(status);
        case ErrorCodes::IncompatibleCollationVersion:
            throw ExceptionFor<ErrorCodes::IncompatibleCollationVersion>(status);
        case ErrorCodes::CollectionIsEmpty:
            throw ExceptionFor<ErrorCodes::CollectionIsEmpty>(status);
        case ErrorCodes::ZoneStillInUse:
            throw ExceptionFor<ErrorCodes::ZoneStillInUse>(status);
        case ErrorCodes::InitialSyncActive:
            throw ExceptionFor<ErrorCodes::InitialSyncActive>(status);
        case ErrorCodes::ViewDepthLimitExceeded:
            throw ExceptionFor<ErrorCodes::ViewDepthLimitExceeded>(status);
        case ErrorCodes::CommandNotSupportedOnView:
            throw ExceptionFor<ErrorCodes::CommandNotSupportedOnView>(status);
        case ErrorCodes::OptionNotSupportedOnView:
            throw ExceptionFor<ErrorCodes::OptionNotSupportedOnView>(status);
        case ErrorCodes::InvalidPipelineOperator:
            throw ExceptionFor<ErrorCodes::InvalidPipelineOperator>(status);
        case ErrorCodes::CommandOnShardedViewNotSupportedOnMongod:
            throw ExceptionFor<ErrorCodes::CommandOnShardedViewNotSupportedOnMongod>(status);
        case ErrorCodes::TooManyMatchingDocuments:
            throw ExceptionFor<ErrorCodes::TooManyMatchingDocuments>(status);
        case ErrorCodes::CannotIndexParallelArrays:
            throw ExceptionFor<ErrorCodes::CannotIndexParallelArrays>(status);
        case ErrorCodes::TransportSessionClosed:
            throw ExceptionFor<ErrorCodes::TransportSessionClosed>(status);
        case ErrorCodes::TransportSessionNotFound:
            throw ExceptionFor<ErrorCodes::TransportSessionNotFound>(status);
        case ErrorCodes::TransportSessionUnknown:
            throw ExceptionFor<ErrorCodes::TransportSessionUnknown>(status);
        case ErrorCodes::QueryPlanKilled:
            throw ExceptionFor<ErrorCodes::QueryPlanKilled>(status);
        case ErrorCodes::FileOpenFailed:
            throw ExceptionFor<ErrorCodes::FileOpenFailed>(status);
        case ErrorCodes::ZoneNotFound:
            throw ExceptionFor<ErrorCodes::ZoneNotFound>(status);
        case ErrorCodes::RangeOverlapConflict:
            throw ExceptionFor<ErrorCodes::RangeOverlapConflict>(status);
        case ErrorCodes::WindowsPdhError:
            throw ExceptionFor<ErrorCodes::WindowsPdhError>(status);
        case ErrorCodes::BadPerfCounterPath:
            throw ExceptionFor<ErrorCodes::BadPerfCounterPath>(status);
        case ErrorCodes::AmbiguousIndexKeyPattern:
            throw ExceptionFor<ErrorCodes::AmbiguousIndexKeyPattern>(status);
        case ErrorCodes::InvalidViewDefinition:
            throw ExceptionFor<ErrorCodes::InvalidViewDefinition>(status);
        case ErrorCodes::ClientMetadataMissingField:
            throw ExceptionFor<ErrorCodes::ClientMetadataMissingField>(status);
        case ErrorCodes::ClientMetadataAppNameTooLarge:
            throw ExceptionFor<ErrorCodes::ClientMetadataAppNameTooLarge>(status);
        case ErrorCodes::ClientMetadataDocumentTooLarge:
            throw ExceptionFor<ErrorCodes::ClientMetadataDocumentTooLarge>(status);
        case ErrorCodes::ClientMetadataCannotBeMutated:
            throw ExceptionFor<ErrorCodes::ClientMetadataCannotBeMutated>(status);
        case ErrorCodes::LinearizableReadConcernError:
            throw ExceptionFor<ErrorCodes::LinearizableReadConcernError>(status);
        case ErrorCodes::IncompatibleServerVersion:
            throw ExceptionFor<ErrorCodes::IncompatibleServerVersion>(status);
        case ErrorCodes::PrimarySteppedDown:
            throw ExceptionFor<ErrorCodes::PrimarySteppedDown>(status);
        case ErrorCodes::MasterSlaveConnectionFailure:
            throw ExceptionFor<ErrorCodes::MasterSlaveConnectionFailure>(status);
        case ErrorCodes::OBSOLETE_BalancerLostDistributedLock:
            throw ExceptionFor<ErrorCodes::OBSOLETE_BalancerLostDistributedLock>(status);
        case ErrorCodes::FailPointEnabled:
            throw ExceptionFor<ErrorCodes::FailPointEnabled>(status);
        case ErrorCodes::NoShardingEnabled:
            throw ExceptionFor<ErrorCodes::NoShardingEnabled>(status);
        case ErrorCodes::BalancerInterrupted:
            throw ExceptionFor<ErrorCodes::BalancerInterrupted>(status);
        case ErrorCodes::ViewPipelineMaxSizeExceeded:
            throw ExceptionFor<ErrorCodes::ViewPipelineMaxSizeExceeded>(status);
        case ErrorCodes::InvalidIndexSpecificationOption:
            throw ExceptionFor<ErrorCodes::InvalidIndexSpecificationOption>(status);
        case ErrorCodes::OBSOLETE_ReceivedOpReplyMessage:
            throw ExceptionFor<ErrorCodes::OBSOLETE_ReceivedOpReplyMessage>(status);
        case ErrorCodes::ReplicaSetMonitorRemoved:
            throw ExceptionFor<ErrorCodes::ReplicaSetMonitorRemoved>(status);
        case ErrorCodes::ChunkRangeCleanupPending:
            throw ExceptionFor<ErrorCodes::ChunkRangeCleanupPending>(status);
        case ErrorCodes::CannotBuildIndexKeys:
            throw ExceptionFor<ErrorCodes::CannotBuildIndexKeys>(status);
        case ErrorCodes::NetworkInterfaceExceededTimeLimit:
            throw ExceptionFor<ErrorCodes::NetworkInterfaceExceededTimeLimit>(status);
        case ErrorCodes::ShardingStateNotInitialized:
            throw ExceptionFor<ErrorCodes::ShardingStateNotInitialized>(status);
        case ErrorCodes::TimeProofMismatch:
            throw ExceptionFor<ErrorCodes::TimeProofMismatch>(status);
        case ErrorCodes::ClusterTimeFailsRateLimiter:
            throw ExceptionFor<ErrorCodes::ClusterTimeFailsRateLimiter>(status);
        case ErrorCodes::NoSuchSession:
            throw ExceptionFor<ErrorCodes::NoSuchSession>(status);
        case ErrorCodes::InvalidUUID:
            throw ExceptionFor<ErrorCodes::InvalidUUID>(status);
        case ErrorCodes::TooManyLocks:
            throw ExceptionFor<ErrorCodes::TooManyLocks>(status);
        case ErrorCodes::StaleClusterTime:
            throw ExceptionFor<ErrorCodes::StaleClusterTime>(status);
        case ErrorCodes::CannotVerifyAndSignLogicalTime:
            throw ExceptionFor<ErrorCodes::CannotVerifyAndSignLogicalTime>(status);
        case ErrorCodes::KeyNotFound:
            throw ExceptionFor<ErrorCodes::KeyNotFound>(status);
        case ErrorCodes::IncompatibleRollbackAlgorithm:
            throw ExceptionFor<ErrorCodes::IncompatibleRollbackAlgorithm>(status);
        case ErrorCodes::DuplicateSession:
            throw ExceptionFor<ErrorCodes::DuplicateSession>(status);
        case ErrorCodes::AuthenticationRestrictionUnmet:
            throw ExceptionFor<ErrorCodes::AuthenticationRestrictionUnmet>(status);
        case ErrorCodes::DatabaseDropPending:
            throw ExceptionFor<ErrorCodes::DatabaseDropPending>(status);
        case ErrorCodes::ElectionInProgress:
            throw ExceptionFor<ErrorCodes::ElectionInProgress>(status);
        case ErrorCodes::IncompleteTransactionHistory:
            throw ExceptionFor<ErrorCodes::IncompleteTransactionHistory>(status);
        case ErrorCodes::UpdateOperationFailed:
            throw ExceptionFor<ErrorCodes::UpdateOperationFailed>(status);
        case ErrorCodes::FTDCPathNotSet:
            throw ExceptionFor<ErrorCodes::FTDCPathNotSet>(status);
        case ErrorCodes::FTDCPathAlreadySet:
            throw ExceptionFor<ErrorCodes::FTDCPathAlreadySet>(status);
        case ErrorCodes::IndexModified:
            throw ExceptionFor<ErrorCodes::IndexModified>(status);
        case ErrorCodes::CloseChangeStream:
            throw ExceptionFor<ErrorCodes::CloseChangeStream>(status);
        case ErrorCodes::IllegalOpMsgFlag:
            throw ExceptionFor<ErrorCodes::IllegalOpMsgFlag>(status);
        case ErrorCodes::QueryFeatureNotAllowed:
            throw ExceptionFor<ErrorCodes::QueryFeatureNotAllowed>(status);
        case ErrorCodes::TransactionTooOld:
            throw ExceptionFor<ErrorCodes::TransactionTooOld>(status);
        case ErrorCodes::AtomicityFailure:
            throw ExceptionFor<ErrorCodes::AtomicityFailure>(status);
        case ErrorCodes::CannotImplicitlyCreateCollection:
            throw ExceptionFor<ErrorCodes::CannotImplicitlyCreateCollection>(status);
        case ErrorCodes::SessionTransferIncomplete:
            throw ExceptionFor<ErrorCodes::SessionTransferIncomplete>(status);
        case ErrorCodes::MustDowngrade:
            throw ExceptionFor<ErrorCodes::MustDowngrade>(status);
        case ErrorCodes::DNSHostNotFound:
            throw ExceptionFor<ErrorCodes::DNSHostNotFound>(status);
        case ErrorCodes::DNSProtocolError:
            throw ExceptionFor<ErrorCodes::DNSProtocolError>(status);
        case ErrorCodes::MaxSubPipelineDepthExceeded:
            throw ExceptionFor<ErrorCodes::MaxSubPipelineDepthExceeded>(status);
        case ErrorCodes::TooManyDocumentSequences:
            throw ExceptionFor<ErrorCodes::TooManyDocumentSequences>(status);
        case ErrorCodes::RetryChangeStream:
            throw ExceptionFor<ErrorCodes::RetryChangeStream>(status);
        case ErrorCodes::SocketException:
            throw ExceptionFor<ErrorCodes::SocketException>(status);
        case ErrorCodes::OBSOLETE_RecvStaleConfig:
            throw ExceptionFor<ErrorCodes::OBSOLETE_RecvStaleConfig>(status);
        case ErrorCodes::CannotGrowDocumentInCappedNamespace:
            throw ExceptionFor<ErrorCodes::CannotGrowDocumentInCappedNamespace>(status);
        case ErrorCodes::NotMaster:
            throw ExceptionFor<ErrorCodes::NotMaster>(status);
        case ErrorCodes::DuplicateKey:
            throw ExceptionFor<ErrorCodes::DuplicateKey>(status);
        case ErrorCodes::InterruptedAtShutdown:
            throw ExceptionFor<ErrorCodes::InterruptedAtShutdown>(status);
        case ErrorCodes::Interrupted:
            throw ExceptionFor<ErrorCodes::Interrupted>(status);
        case ErrorCodes::InterruptedDueToReplStateChange:
            throw ExceptionFor<ErrorCodes::InterruptedDueToReplStateChange>(status);
        case ErrorCodes::BackgroundOperationInProgressForDatabase:
            throw ExceptionFor<ErrorCodes::BackgroundOperationInProgressForDatabase>(status);
        case ErrorCodes::BackgroundOperationInProgressForNamespace:
            throw ExceptionFor<ErrorCodes::BackgroundOperationInProgressForNamespace>(status);
        case ErrorCodes::OBSOLETE_PrepareConfigsFailed:
            throw ExceptionFor<ErrorCodes::OBSOLETE_PrepareConfigsFailed>(status);
        case ErrorCodes::DatabaseDifferCase:
            throw ExceptionFor<ErrorCodes::DatabaseDifferCase>(status);
        case ErrorCodes::ShardKeyTooBig:
            throw ExceptionFor<ErrorCodes::ShardKeyTooBig>(status);
        case ErrorCodes::StaleConfig:
            throw ExceptionFor<ErrorCodes::StaleConfig>(status);
        case ErrorCodes::NotMasterNoSlaveOk:
            throw ExceptionFor<ErrorCodes::NotMasterNoSlaveOk>(status);
        case ErrorCodes::NotMasterOrSecondary:
            throw ExceptionFor<ErrorCodes::NotMasterOrSecondary>(status);
        case ErrorCodes::OutOfDiskSpace:
            throw ExceptionFor<ErrorCodes::OutOfDiskSpace>(status);
        case ErrorCodes::KeyTooLong:
            throw ExceptionFor<ErrorCodes::KeyTooLong>(status);
        default:
            throw NonspecificAssertionException(status);
    }
}

}  // namespace mongo
