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

var ErrorCodes = {
    'OK': 0,
    'InternalError': 1,
    'BadValue': 2,
    'OBSOLETE_DuplicateKey': 3,
    'NoSuchKey': 4,
    'GraphContainsCycle': 5,
    'HostUnreachable': 6,
    'HostNotFound': 7,
    'UnknownError': 8,
    'FailedToParse': 9,
    'CannotMutateObject': 10,
    'UserNotFound': 11,
    'UnsupportedFormat': 12,
    'Unauthorized': 13,
    'TypeMismatch': 14,
    'Overflow': 15,
    'InvalidLength': 16,
    'ProtocolError': 17,
    'AuthenticationFailed': 18,
    'CannotReuseObject': 19,
    'IllegalOperation': 20,
    'EmptyArrayOperation': 21,
    'InvalidBSON': 22,
    'AlreadyInitialized': 23,
    'LockTimeout': 24,
    'RemoteValidationError': 25,
    'NamespaceNotFound': 26,
    'IndexNotFound': 27,
    'PathNotViable': 28,
    'NonExistentPath': 29,
    'InvalidPath': 30,
    'RoleNotFound': 31,
    'RolesNotRelated': 32,
    'PrivilegeNotFound': 33,
    'CannotBackfillArray': 34,
    'UserModificationFailed': 35,
    'RemoteChangeDetected': 36,
    'FileRenameFailed': 37,
    'FileNotOpen': 38,
    'FileStreamFailed': 39,
    'ConflictingUpdateOperators': 40,
    'FileAlreadyOpen': 41,
    'LogWriteFailed': 42,
    'CursorNotFound': 43,
    'UserDataInconsistent': 45,
    'LockBusy': 46,
    'NoMatchingDocument': 47,
    'NamespaceExists': 48,
    'InvalidRoleModification': 49,
    'ExceededTimeLimit': 50,
    'ManualInterventionRequired': 51,
    'DollarPrefixedFieldName': 52,
    'InvalidIdField': 53,
    'NotSingleValueField': 54,
    'InvalidDBRef': 55,
    'EmptyFieldName': 56,
    'DottedFieldName': 57,
    'RoleModificationFailed': 58,
    'CommandNotFound': 59,
    'OBSOLETE_DatabaseNotFound': 60,
    'ShardKeyNotFound': 61,
    'OplogOperationUnsupported': 62,
    'StaleShardVersion': 63,
    'WriteConcernFailed': 64,
    'MultipleErrorsOccurred': 65,
    'ImmutableField': 66,
    'CannotCreateIndex': 67,
    'IndexAlreadyExists': 68,
    'AuthSchemaIncompatible': 69,
    'ShardNotFound': 70,
    'ReplicaSetNotFound': 71,
    'InvalidOptions': 72,
    'InvalidNamespace': 73,
    'NodeNotFound': 74,
    'WriteConcernLegacyOK': 75,
    'NoReplicationEnabled': 76,
    'OperationIncomplete': 77,
    'CommandResultSchemaViolation': 78,
    'UnknownReplWriteConcern': 79,
    'RoleDataInconsistent': 80,
    'NoMatchParseContext': 81,
    'NoProgressMade': 82,
    'RemoteResultsUnavailable': 83,
    'DuplicateKeyValue': 84,
    'IndexOptionsConflict': 85,
    'IndexKeySpecsConflict': 86,
    'CannotSplit': 87,
    'SplitFailed_OBSOLETE': 88,
    'NetworkTimeout': 89,
    'CallbackCanceled': 90,
    'ShutdownInProgress': 91,
    'SecondaryAheadOfPrimary': 92,
    'InvalidReplicaSetConfig': 93,
    'NotYetInitialized': 94,
    'NotSecondary': 95,
    'OperationFailed': 96,
    'NoProjectionFound': 97,
    'DBPathInUse': 98,
    'CannotSatisfyWriteConcern': 100,
    'OutdatedClient': 101,
    'IncompatibleAuditMetadata': 102,
    'NewReplicaSetConfigurationIncompatible': 103,
    'NodeNotElectable': 104,
    'IncompatibleShardingMetadata': 105,
    'DistributedClockSkewed': 106,
    'LockFailed': 107,
    'InconsistentReplicaSetNames': 108,
    'ConfigurationInProgress': 109,
    'CannotInitializeNodeWithData': 110,
    'NotExactValueField': 111,
    'WriteConflict': 112,
    'InitialSyncFailure': 113,
    'InitialSyncOplogSourceMissing': 114,
    'CommandNotSupported': 115,
    'DocTooLargeForCapped': 116,
    'ConflictingOperationInProgress': 117,
    'NamespaceNotSharded': 118,
    'InvalidSyncSource': 119,
    'OplogStartMissing': 120,
    'DocumentValidationFailure': 121,
    'OBSOLETE_ReadAfterOptimeTimeout': 122,
    'NotAReplicaSet': 123,
    'IncompatibleElectionProtocol': 124,
    'CommandFailed': 125,
    'RPCProtocolNegotiationFailed': 126,
    'UnrecoverableRollbackError': 127,
    'LockNotFound': 128,
    'LockStateChangeFailed': 129,
    'SymbolNotFound': 130,
    'RLPInitializationFailed': 131,
    'OBSOLETE_ConfigServersInconsistent': 132,
    'FailedToSatisfyReadPreference': 133,
    'ReadConcernMajorityNotAvailableYet': 134,
    'StaleTerm': 135,
    'CappedPositionLost': 136,
    'IncompatibleShardingConfigVersion': 137,
    'RemoteOplogStale': 138,
    'JSInterpreterFailure': 139,
    'InvalidSSLConfiguration': 140,
    'SSLHandshakeFailed': 141,
    'JSUncatchableError': 142,
    'CursorInUse': 143,
    'IncompatibleCatalogManager': 144,
    'PooledConnectionsDropped': 145,
    'ExceededMemoryLimit': 146,
    'ZLibError': 147,
    'ReadConcernMajorityNotEnabled': 148,
    'NoConfigMaster': 149,
    'StaleEpoch': 150,
    'OperationCannotBeBatched': 151,
    'OplogOutOfOrder': 152,
    'ChunkTooBig': 153,
    'InconsistentShardIdentity': 154,
    'CannotApplyOplogWhilePrimary': 155,
    'NeedsDocumentMove': 156,
    'CanRepairToDowngrade': 157,
    'MustUpgrade': 158,
    'DurationOverflow': 159,
    'MaxStalenessOutOfRange': 160,
    'IncompatibleCollationVersion': 161,
    'CollectionIsEmpty': 162,
    'ZoneStillInUse': 163,
    'InitialSyncActive': 164,
    'ViewDepthLimitExceeded': 165,
    'CommandNotSupportedOnView': 166,
    'OptionNotSupportedOnView': 167,
    'InvalidPipelineOperator': 168,
    'CommandOnShardedViewNotSupportedOnMongod': 169,
    'TooManyMatchingDocuments': 170,
    'CannotIndexParallelArrays': 171,
    'TransportSessionClosed': 172,
    'TransportSessionNotFound': 173,
    'TransportSessionUnknown': 174,
    'QueryPlanKilled': 175,
    'FileOpenFailed': 176,
    'ZoneNotFound': 177,
    'RangeOverlapConflict': 178,
    'WindowsPdhError': 179,
    'BadPerfCounterPath': 180,
    'AmbiguousIndexKeyPattern': 181,
    'InvalidViewDefinition': 182,
    'ClientMetadataMissingField': 183,
    'ClientMetadataAppNameTooLarge': 184,
    'ClientMetadataDocumentTooLarge': 185,
    'ClientMetadataCannotBeMutated': 186,
    'LinearizableReadConcernError': 187,
    'IncompatibleServerVersion': 188,
    'PrimarySteppedDown': 189,
    'MasterSlaveConnectionFailure': 190,
    'OBSOLETE_BalancerLostDistributedLock': 191,
    'FailPointEnabled': 192,
    'NoShardingEnabled': 193,
    'BalancerInterrupted': 194,
    'ViewPipelineMaxSizeExceeded': 195,
    'InvalidIndexSpecificationOption': 197,
    'OBSOLETE_ReceivedOpReplyMessage': 198,
    'ReplicaSetMonitorRemoved': 199,
    'ChunkRangeCleanupPending': 200,
    'CannotBuildIndexKeys': 201,
    'NetworkInterfaceExceededTimeLimit': 202,
    'ShardingStateNotInitialized': 203,
    'TimeProofMismatch': 204,
    'ClusterTimeFailsRateLimiter': 205,
    'NoSuchSession': 206,
    'InvalidUUID': 207,
    'TooManyLocks': 208,
    'StaleClusterTime': 209,
    'CannotVerifyAndSignLogicalTime': 210,
    'KeyNotFound': 211,
    'IncompatibleRollbackAlgorithm': 212,
    'DuplicateSession': 213,
    'AuthenticationRestrictionUnmet': 214,
    'DatabaseDropPending': 215,
    'ElectionInProgress': 216,
    'IncompleteTransactionHistory': 217,
    'UpdateOperationFailed': 218,
    'FTDCPathNotSet': 219,
    'FTDCPathAlreadySet': 220,
    'IndexModified': 221,
    'CloseChangeStream': 222,
    'IllegalOpMsgFlag': 223,
    'QueryFeatureNotAllowed': 224,
    'TransactionTooOld': 225,
    'AtomicityFailure': 226,
    'CannotImplicitlyCreateCollection': 227,
    'SessionTransferIncomplete': 228,
    'MustDowngrade': 229,
    'DNSHostNotFound': 230,
    'DNSProtocolError': 231,
    'MaxSubPipelineDepthExceeded': 232,
    'TooManyDocumentSequences': 233,
    'RetryChangeStream': 234,
    'SocketException': 9001,
    'OBSOLETE_RecvStaleConfig': 9996,
    'CannotGrowDocumentInCappedNamespace': 10003,
    'NotMaster': 10107,
    'DuplicateKey': 11000,
    'InterruptedAtShutdown': 11600,
    'Interrupted': 11601,
    'InterruptedDueToReplStateChange': 11602,
    'BackgroundOperationInProgressForDatabase': 12586,
    'BackgroundOperationInProgressForNamespace': 12587,
    'OBSOLETE_PrepareConfigsFailed': 13104,
    'DatabaseDifferCase': 13297,
    'ShardKeyTooBig': 13334,
    'StaleConfig': 13388,
    'NotMasterNoSlaveOk': 13435,
    'NotMasterOrSecondary': 13436,
    'OutOfDiskSpace': 14031,
    'KeyTooLong': 17280,
};

var ErrorCodeStrings = {
    0: 'OK',
    1: 'InternalError',
    2: 'BadValue',
    3: 'OBSOLETE_DuplicateKey',
    4: 'NoSuchKey',
    5: 'GraphContainsCycle',
    6: 'HostUnreachable',
    7: 'HostNotFound',
    8: 'UnknownError',
    9: 'FailedToParse',
    10: 'CannotMutateObject',
    11: 'UserNotFound',
    12: 'UnsupportedFormat',
    13: 'Unauthorized',
    14: 'TypeMismatch',
    15: 'Overflow',
    16: 'InvalidLength',
    17: 'ProtocolError',
    18: 'AuthenticationFailed',
    19: 'CannotReuseObject',
    20: 'IllegalOperation',
    21: 'EmptyArrayOperation',
    22: 'InvalidBSON',
    23: 'AlreadyInitialized',
    24: 'LockTimeout',
    25: 'RemoteValidationError',
    26: 'NamespaceNotFound',
    27: 'IndexNotFound',
    28: 'PathNotViable',
    29: 'NonExistentPath',
    30: 'InvalidPath',
    31: 'RoleNotFound',
    32: 'RolesNotRelated',
    33: 'PrivilegeNotFound',
    34: 'CannotBackfillArray',
    35: 'UserModificationFailed',
    36: 'RemoteChangeDetected',
    37: 'FileRenameFailed',
    38: 'FileNotOpen',
    39: 'FileStreamFailed',
    40: 'ConflictingUpdateOperators',
    41: 'FileAlreadyOpen',
    42: 'LogWriteFailed',
    43: 'CursorNotFound',
    45: 'UserDataInconsistent',
    46: 'LockBusy',
    47: 'NoMatchingDocument',
    48: 'NamespaceExists',
    49: 'InvalidRoleModification',
    50: 'ExceededTimeLimit',
    51: 'ManualInterventionRequired',
    52: 'DollarPrefixedFieldName',
    53: 'InvalidIdField',
    54: 'NotSingleValueField',
    55: 'InvalidDBRef',
    56: 'EmptyFieldName',
    57: 'DottedFieldName',
    58: 'RoleModificationFailed',
    59: 'CommandNotFound',
    60: 'OBSOLETE_DatabaseNotFound',
    61: 'ShardKeyNotFound',
    62: 'OplogOperationUnsupported',
    63: 'StaleShardVersion',
    64: 'WriteConcernFailed',
    65: 'MultipleErrorsOccurred',
    66: 'ImmutableField',
    67: 'CannotCreateIndex',
    68: 'IndexAlreadyExists',
    69: 'AuthSchemaIncompatible',
    70: 'ShardNotFound',
    71: 'ReplicaSetNotFound',
    72: 'InvalidOptions',
    73: 'InvalidNamespace',
    74: 'NodeNotFound',
    75: 'WriteConcernLegacyOK',
    76: 'NoReplicationEnabled',
    77: 'OperationIncomplete',
    78: 'CommandResultSchemaViolation',
    79: 'UnknownReplWriteConcern',
    80: 'RoleDataInconsistent',
    81: 'NoMatchParseContext',
    82: 'NoProgressMade',
    83: 'RemoteResultsUnavailable',
    84: 'DuplicateKeyValue',
    85: 'IndexOptionsConflict',
    86: 'IndexKeySpecsConflict',
    87: 'CannotSplit',
    88: 'SplitFailed_OBSOLETE',
    89: 'NetworkTimeout',
    90: 'CallbackCanceled',
    91: 'ShutdownInProgress',
    92: 'SecondaryAheadOfPrimary',
    93: 'InvalidReplicaSetConfig',
    94: 'NotYetInitialized',
    95: 'NotSecondary',
    96: 'OperationFailed',
    97: 'NoProjectionFound',
    98: 'DBPathInUse',
    100: 'CannotSatisfyWriteConcern',
    101: 'OutdatedClient',
    102: 'IncompatibleAuditMetadata',
    103: 'NewReplicaSetConfigurationIncompatible',
    104: 'NodeNotElectable',
    105: 'IncompatibleShardingMetadata',
    106: 'DistributedClockSkewed',
    107: 'LockFailed',
    108: 'InconsistentReplicaSetNames',
    109: 'ConfigurationInProgress',
    110: 'CannotInitializeNodeWithData',
    111: 'NotExactValueField',
    112: 'WriteConflict',
    113: 'InitialSyncFailure',
    114: 'InitialSyncOplogSourceMissing',
    115: 'CommandNotSupported',
    116: 'DocTooLargeForCapped',
    117: 'ConflictingOperationInProgress',
    118: 'NamespaceNotSharded',
    119: 'InvalidSyncSource',
    120: 'OplogStartMissing',
    121: 'DocumentValidationFailure',
    122: 'OBSOLETE_ReadAfterOptimeTimeout',
    123: 'NotAReplicaSet',
    124: 'IncompatibleElectionProtocol',
    125: 'CommandFailed',
    126: 'RPCProtocolNegotiationFailed',
    127: 'UnrecoverableRollbackError',
    128: 'LockNotFound',
    129: 'LockStateChangeFailed',
    130: 'SymbolNotFound',
    131: 'RLPInitializationFailed',
    132: 'OBSOLETE_ConfigServersInconsistent',
    133: 'FailedToSatisfyReadPreference',
    134: 'ReadConcernMajorityNotAvailableYet',
    135: 'StaleTerm',
    136: 'CappedPositionLost',
    137: 'IncompatibleShardingConfigVersion',
    138: 'RemoteOplogStale',
    139: 'JSInterpreterFailure',
    140: 'InvalidSSLConfiguration',
    141: 'SSLHandshakeFailed',
    142: 'JSUncatchableError',
    143: 'CursorInUse',
    144: 'IncompatibleCatalogManager',
    145: 'PooledConnectionsDropped',
    146: 'ExceededMemoryLimit',
    147: 'ZLibError',
    148: 'ReadConcernMajorityNotEnabled',
    149: 'NoConfigMaster',
    150: 'StaleEpoch',
    151: 'OperationCannotBeBatched',
    152: 'OplogOutOfOrder',
    153: 'ChunkTooBig',
    154: 'InconsistentShardIdentity',
    155: 'CannotApplyOplogWhilePrimary',
    156: 'NeedsDocumentMove',
    157: 'CanRepairToDowngrade',
    158: 'MustUpgrade',
    159: 'DurationOverflow',
    160: 'MaxStalenessOutOfRange',
    161: 'IncompatibleCollationVersion',
    162: 'CollectionIsEmpty',
    163: 'ZoneStillInUse',
    164: 'InitialSyncActive',
    165: 'ViewDepthLimitExceeded',
    166: 'CommandNotSupportedOnView',
    167: 'OptionNotSupportedOnView',
    168: 'InvalidPipelineOperator',
    169: 'CommandOnShardedViewNotSupportedOnMongod',
    170: 'TooManyMatchingDocuments',
    171: 'CannotIndexParallelArrays',
    172: 'TransportSessionClosed',
    173: 'TransportSessionNotFound',
    174: 'TransportSessionUnknown',
    175: 'QueryPlanKilled',
    176: 'FileOpenFailed',
    177: 'ZoneNotFound',
    178: 'RangeOverlapConflict',
    179: 'WindowsPdhError',
    180: 'BadPerfCounterPath',
    181: 'AmbiguousIndexKeyPattern',
    182: 'InvalidViewDefinition',
    183: 'ClientMetadataMissingField',
    184: 'ClientMetadataAppNameTooLarge',
    185: 'ClientMetadataDocumentTooLarge',
    186: 'ClientMetadataCannotBeMutated',
    187: 'LinearizableReadConcernError',
    188: 'IncompatibleServerVersion',
    189: 'PrimarySteppedDown',
    190: 'MasterSlaveConnectionFailure',
    191: 'OBSOLETE_BalancerLostDistributedLock',
    192: 'FailPointEnabled',
    193: 'NoShardingEnabled',
    194: 'BalancerInterrupted',
    195: 'ViewPipelineMaxSizeExceeded',
    197: 'InvalidIndexSpecificationOption',
    198: 'OBSOLETE_ReceivedOpReplyMessage',
    199: 'ReplicaSetMonitorRemoved',
    200: 'ChunkRangeCleanupPending',
    201: 'CannotBuildIndexKeys',
    202: 'NetworkInterfaceExceededTimeLimit',
    203: 'ShardingStateNotInitialized',
    204: 'TimeProofMismatch',
    205: 'ClusterTimeFailsRateLimiter',
    206: 'NoSuchSession',
    207: 'InvalidUUID',
    208: 'TooManyLocks',
    209: 'StaleClusterTime',
    210: 'CannotVerifyAndSignLogicalTime',
    211: 'KeyNotFound',
    212: 'IncompatibleRollbackAlgorithm',
    213: 'DuplicateSession',
    214: 'AuthenticationRestrictionUnmet',
    215: 'DatabaseDropPending',
    216: 'ElectionInProgress',
    217: 'IncompleteTransactionHistory',
    218: 'UpdateOperationFailed',
    219: 'FTDCPathNotSet',
    220: 'FTDCPathAlreadySet',
    221: 'IndexModified',
    222: 'CloseChangeStream',
    223: 'IllegalOpMsgFlag',
    224: 'QueryFeatureNotAllowed',
    225: 'TransactionTooOld',
    226: 'AtomicityFailure',
    227: 'CannotImplicitlyCreateCollection',
    228: 'SessionTransferIncomplete',
    229: 'MustDowngrade',
    230: 'DNSHostNotFound',
    231: 'DNSProtocolError',
    232: 'MaxSubPipelineDepthExceeded',
    233: 'TooManyDocumentSequences',
    234: 'RetryChangeStream',
    9001: 'SocketException',
    9996: 'OBSOLETE_RecvStaleConfig',
    10003: 'CannotGrowDocumentInCappedNamespace',
    10107: 'NotMaster',
    11000: 'DuplicateKey',
    11600: 'InterruptedAtShutdown',
    11601: 'Interrupted',
    11602: 'InterruptedDueToReplStateChange',
    12586: 'BackgroundOperationInProgressForDatabase',
    12587: 'BackgroundOperationInProgressForNamespace',
    13104: 'OBSOLETE_PrepareConfigsFailed',
    13297: 'DatabaseDifferCase',
    13334: 'ShardKeyTooBig',
    13388: 'StaleConfig',
    13435: 'NotMasterNoSlaveOk',
    13436: 'NotMasterOrSecondary',
    14031: 'OutOfDiskSpace',
    17280: 'KeyTooLong',
};

ErrorCodes.isNetworkError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'HostUnreachable':
            return true;
        case 'HostNotFound':
            return true;
        case 'NetworkTimeout':
            return true;
        case 'SocketException':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isInterruption = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'Interrupted':
            return true;
        case 'InterruptedAtShutdown':
            return true;
        case 'InterruptedDueToReplStateChange':
            return true;
        case 'ExceededTimeLimit':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isNotMasterError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'NotMaster':
            return true;
        case 'NotMasterNoSlaveOk':
            return true;
        case 'NotMasterOrSecondary':
            return true;
        case 'InterruptedDueToReplStateChange':
            return true;
        case 'PrimarySteppedDown':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isStaleShardingError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'StaleConfig':
            return true;
        case 'StaleShardVersion':
            return true;
        case 'StaleEpoch':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isWriteConcernError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'WriteConcernFailed':
            return true;
        case 'WriteConcernLegacyOK':
            return true;
        case 'UnknownReplWriteConcern':
            return true;
        case 'CannotSatisfyWriteConcern':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isShutdownError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'ShutdownInProgress':
            return true;
        case 'InterruptedAtShutdown':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isConnectionFatalMessageParseError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'IllegalOpMsgFlag':
            return true;
        case 'TooManyDocumentSequences':
            return true;
        default:
            return false;
    }
};
ErrorCodes.isExceededTimeLimitError = function(err) {
    'use strict';

    var error;
    if (typeof err === 'string') {
        error = err;
    } else if (typeof err === 'number') {
        error = ErrorCodeStrings[err];
    }
    switch (error) {
        case 'ExceededTimeLimit':
            return true;
        case 'NetworkInterfaceExceededTimeLimit':
            return true;
        default:
            return false;
    }
};
