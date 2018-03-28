/*    Copyright 2012 10gen Inc.
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

#include "mongo/base/initializer.h"

#include "mongo/base/global_initializer.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/quick_exit.h"
#include <iostream>

namespace mongo {

Initializer::Initializer() {}
Initializer::~Initializer() {}

/*
yang test Initializer::execute:OIDGeneration
yang test Initializer::execute:ValidateLocale
yang test Initializer::execute:EnableVersionInfo
yang test Initializer::execute:GlobalLogManager
yang test Initializer::execute:SystemInfo
yang test Initializer::execute:TcmallocConfigurationDefaults
yang test Initializer::execute:BeginStartupOptionHandling
yang test Initializer::execute:BeginStartupOptionRegistration
yang test Initializer::execute:BeginGeneralStartupOptionRegistration
yang test Initializer::execute:MongodOptions_Register
yang test Initializer::execute:EndGeneralStartupOptionRegistration
yang test Initializer::execute:SASLOptions_Register
yang test Initializer::execute:WiredTigerOptions_Register
yang test Initializer::execute:EndStartupOptionRegistration
yang test Initializer::execute:OptionsParseUseStrict
yang test Initializer::execute:AuthzSchemaParameter
yang test Initializer::execute:BeginStartupOptionParsing
yang test Initializer::execute:StartupOptions_Parse
yang test Initializer::execute:EndStartupOptionParsing
yang test Initializer::execute:BeginStartupOptionValidation
yang test Initializer::execute:WiredTigerOptions_Validate
yang test Initializer::execute:FailPointRegistry
yang test Initializer::execute:throwSockExcep
yang test Initializer::execute:disableKeyGeneration
yang test Initializer::execute:maxKeyRefreshWaitTimeOverrideMS
yang test Initializer::execute:scheduleIntoPoolSpinsUntilThreadPoolShutsDown
yang test Initializer::execute:failAsyncConfigChangeHook
yang test Initializer::execute:checkForInterruptFail
yang test Initializer::execute:disableAwaitDataForGetMoreCmd
yang test Initializer::execute:keepCursorPinnedDuringGetMore
yang test Initializer::execute:failApplyChunkOps
yang test Initializer::execute:initialSyncHangCollectionClonerAfterHandlingBatchResponse
yang test Initializer::execute:initialSyncHangDuringCollectionClone
yang test Initializer::execute:crashOnShutdown
yang test Initializer::execute:initialSyncHangAfterListCollections
yang test Initializer::execute:stopReplProducer
yang test Initializer::execute:rsSyncApplyStop
yang test Initializer::execute:initialSyncHangBeforeFinish
yang test Initializer::execute:failInitSyncWithBufferedEntriesLeft
yang test Initializer::execute:blockHeartbeatStepdown
yang test Initializer::execute:stepdownHangBeforePerformingPostMemberStateUpdateActions
yang test Initializer::execute:blockHeartbeatReconfigFinish
yang test Initializer::execute:setDistLockTimeout
yang test Initializer::execute:hangBeforeLeavingCriticalSection
yang test Initializer::execute:failMigrationCommit
yang test Initializer::execute:failMigrationReceivedOutOfRangeOperation
yang test Initializer::execute:failMigrationLeaveOrphans
yang test Initializer::execute:migrateThreadHangAtStep6
yang test Initializer::execute:migrateThreadHangAtStep4
yang test Initializer::execute:migrateThreadHangAtStep3
yang test Initializer::execute:transitionToPrimaryHangBeforeTakingGlobalExclusiveLock
yang test Initializer::execute:migrateThreadHangAtStep2
yang test Initializer::execute:doNotRefreshRecipientAfterCommit
yang test Initializer::execute:onPrimaryTransactionalWrite
yang test Initializer::execute:featureCompatibilityUpgrade
yang test Initializer::execute:disableMaxSyncSourceLagSecs
yang test Initializer::execute:failCollectionInserts
yang test Initializer::execute:WTWriteConflictExceptionForReads
yang test Initializer::execute:dummy
yang test Initializer::execute:moveChunkHangAtStep4
yang test Initializer::execute:suspendRangeDeletion
yang test Initializer::execute:rsDelayHeartbeatResponse
yang test Initializer::execute:disableSnapshotting
yang test Initializer::execute:rollbackHangThenFailAfterWritingMinValid
yang test Initializer::execute:moveChunkHangAtStep5
yang test Initializer::execute:migrationCommitNetworkError
yang test Initializer::execute:moveChunkHangAtStep7
yang test Initializer::execute:maxTimeAlwaysTimeOut
yang test Initializer::execute:initialSyncHangBeforeCollectionClone
yang test Initializer::execute:setYieldAllLocksWait
yang test Initializer::execute:WTWriteConflictException
yang test Initializer::execute:hangAfterStartingIndexBuild
yang test Initializer::execute:hangBeforeDatabaseUpgrade
yang test Initializer::execute:hangAfterStartingIndexBuildUnlocked
yang test Initializer::execute:moveChunkHangAtStep1
yang test Initializer::execute:respondWithNotPrimaryInCommandDispatch
yang test Initializer::execute:applyOpsPauseBetweenOperations
yang test Initializer::execute:WTEmulateOutOfOrderNextIndexKey
yang test Initializer::execute:moveChunkHangAtStep2
yang test Initializer::execute:moveChunkHangAtStep6
yang test Initializer::execute:moveChunkHangAtStep3
yang test Initializer::execute:recordNeedsFetchFail
yang test Initializer::execute:initialSyncHangBeforeGettingMissingDocument
yang test Initializer::execute:skipCheckingForNotMasterInCommandDispatch
yang test Initializer::execute:rsStopGetMore
yang test Initializer::execute:NetworkInterfaceASIOasyncRunCommandFail
yang test Initializer::execute:featureCompatibilityDowngrade
yang test Initializer::execute:failAllRemoves
yang test Initializer::execute:migrateThreadHangAtStep5
yang test Initializer::execute:hangBeforeWaitingForWriteConcern
yang test Initializer::execute:shutdownAtStartup
yang test Initializer::execute:initialSyncHangBeforeCopyingDatabases
yang test Initializer::execute:failAllUpdates
yang test Initializer::execute:planExecutorAlwaysFails
yang test Initializer::execute:mr_killop_test_fp
yang test Initializer::execute:validateCmdCollectionNotValid
yang test Initializer::execute:rsStopGetMoreCmd
yang test Initializer::execute:failCollectionUpdates
yang test Initializer::execute:initialSyncHangBeforeListCollections
yang test Initializer::execute:rollbackHangBeforeFinish
yang test Initializer::execute:hangBeforeLoggingCreateCollection
yang test Initializer::execute:crashAfterStartingIndexBuild
yang test Initializer::execute:maxTimeNeverTimeOut
yang test Initializer::execute:failInitialSyncWithBadHost
yang test Initializer::execute:rollbackHangBeforeStart
yang test Initializer::execute:migrationCommitVersionError
yang test Initializer::execute:allocateDiskFull
yang test Initializer::execute:WTPausePrimaryOplogDurabilityLoop
yang test Initializer::execute:skipIndexCreateFieldNameValidation
yang test Initializer::execute:migrateThreadHangAtStep1
yang test Initializer::execute:setYieldAllLocksHang
yang test Initializer::execute:failReceivedGetmore
yang test Initializer::execute:failAllInserts
yang test Initializer::execute:setAutoGetCollectionWait
yang test Initializer::execute:AllFailPointsRegistered
yang test Initializer::execute:MongodOptions
yang test Initializer::execute:EndStartupOptionValidation
yang test Initializer::execute:BeginStartupOptionSetup
yang test Initializer::execute:ServerOptions_Setup
yang test Initializer::execute:EndStartupOptionSetup
yang test Initializer::execute:BeginStartupOptionStorage
yang test Initializer::execute:WiredTigerOptions_Store
yang test Initializer::execute:SASLOptions_Store
yang test Initializer::execute:MongodOptions_Store
yang test Initializer::execute:EndStartupOptionStorage
yang test Initializer::execute:EndStartupOptionHandling
yang test Initializer::execute:ForkServer
yang test Initializer::execute:StartHeapProfiling
yang test Initializer::execute:ServerLogRedirection
yang test Initializer::execute:default
yang test Initializer::execute:SystemTickSourceInit
yang test Initializer::execute:ThreadNameInitializer
yang test Initializer::execute:RamLogCatalog
yang test Initializer::execute:LogstreamBuilder
yang test Initializer::execute:EnsureIosBaseInitConstructed
yang test Initializer::execute:addAliasToDocSourceParserMap_sortByCount
yang test Initializer::execute:addToGranularityRounderMap_R10
yang test Initializer::execute:addToDocSourceParserMap_replaceRoot
yang test Initializer::execute:addToDocSourceParserMap_currentOp
yang test Initializer::execute:addToExpressionParserMap_and
yang test Initializer::execute:addAliasToDocSourceParserMap_count
yang test Initializer::execute:addToExpressionParserMap_lt
yang test Initializer::execute:LoadICUData
yang test Initializer::execute:addToExpressionParserMap_meta
yang test Initializer::execute:addAliasToDocSourceParserMap_changeStream
yang test Initializer::execute:GenerateInstanceId
yang test Initializer::execute:addToExpressionParserMap_indexOfBytes
yang test Initializer::execute:InitializeCollectionInfoCacheFactory
yang test Initializer::execute:ExtractSOMap
yang test Initializer::execute:RegisterClearLogCmd
yang test Initializer::execute:addToDocSourceParserMap_addFields
yang test Initializer::execute:addToExpressionParserMap_lte
yang test Initializer::execute:addToDocSourceParserMap__internalInhibitOptimization
yang test Initializer::execute:addToDocSourceParserMap_group
yang test Initializer::execute:addToExpressionParserMap_dateFromString
yang test Initializer::execute:addToExpressionParserMap_toLower
yang test Initializer::execute:addToExpressionParserMap_mod
yang test Initializer::execute:addToGranularityRounderMap_E96
yang test Initializer::execute:languageDanV1
yang test Initializer::execute:addToExpressionParserMap_filter
yang test Initializer::execute:InitializeMultiIndexBlockFactory
yang test Initializer::execute:languageItalianV1
yang test Initializer::execute:addToDocSourceParserMap_sort
yang test Initializer::execute:languageTurkishV1
yang test Initializer::execute:languageTurV1
yang test Initializer::execute:languageTrV1
yang test Initializer::execute:languageSwedishV1
yang test Initializer::execute:languageSpanishV1
yang test Initializer::execute:languageGerV1
yang test Initializer::execute:languageFrV1
yang test Initializer::execute:languageRonV1
yang test Initializer::execute:languageEnglishV1
yang test Initializer::execute:languageEngV1
yang test Initializer::execute:languagePortugueseV1
yang test Initializer::execute:languageSvV1
yang test Initializer::execute:languageNoneV1
yang test Initializer::execute:languageEslV1
yang test Initializer::execute:languageDaV1
yang test Initializer::execute:languageSpaV1
yang test Initializer::execute:FTSRegisterV2LanguagesAndLater
yang test Initializer::execute:languageFinnishV1
yang test Initializer::execute:languageGermanV1
yang test Initializer::execute:languageDeV1
yang test Initializer::execute:languageEnV1
yang test Initializer::execute:languageHungarianV1
yang test Initializer::execute:languageRusV1
yang test Initializer::execute:languageFrenchV1
yang test Initializer::execute:languageEsV1
yang test Initializer::execute:languageSweV1
yang test Initializer::execute:languageRussianV1
yang test Initializer::execute:languageDeuV1
yang test Initializer::execute:languageDutV1
yang test Initializer::execute:languageHuV1
yang test Initializer::execute:languageHunV1
yang test Initializer::execute:languageDutchV1
yang test Initializer::execute:languageItV1
yang test Initializer::execute:languageItaV1
yang test Initializer::execute:languageNorV1
yang test Initializer::execute:languageNlV1
yang test Initializer::execute:languageNoV1
yang test Initializer::execute:languagePorterV1
yang test Initializer::execute:languageFinV1
yang test Initializer::execute:languageNldV1
yang test Initializer::execute:languageNorwegianV1
yang test Initializer::execute:languageDanishV1
yang test Initializer::execute:languagePtV1
yang test Initializer::execute:languagePorV1
yang test Initializer::execute:languageRoV1
yang test Initializer::execute:languageRomanianV1
yang test Initializer::execute:languageFreV1
yang test Initializer::execute:languageRuV1
yang test Initializer::execute:languageFiV1
yang test Initializer::execute:languageFraV1
yang test Initializer::execute:languageRumV1
yang test Initializer::execute:FTSAllLanguagesRegistered
yang test Initializer::execute:addToAccumulatorFactoryMap_stdDevPop
yang test Initializer::execute:addToDocSourceParserMap_skip
yang test Initializer::execute:InitializeJournalingParams
yang test Initializer::execute:FTSRegisterLanguageAliases
yang test Initializer::execute:NoopMessageCompressorInit
yang test Initializer::execute:AuthIndexKeyPatterns
yang test Initializer::execute:RegisterRefreshLogicalSessionCacheNowCommand
yang test Initializer::execute:addToExpressionParserMap_substr
yang test Initializer::execute:addToExpressionParserMap_toUpper
yang test Initializer::execute:RecordBlockSupported
yang test Initializer::execute:addToDocSourceParserMap_project
yang test Initializer::execute:addToExpressionParserMap_or
yang test Initializer::execute:SetupInternalSecurityUser
yang test Initializer::execute:addToExpressionParserMap_month
yang test Initializer::execute:InitializeFixIndexKeyImpl
yang test Initializer::execute:addToExpressionParserMap_allElementsTrue
yang test Initializer::execute:addToExpressionParserMap_multiply
yang test Initializer::execute:SetInitRsOplogBackgroundThreadCallback
yang test Initializer::execute:ClusterBalancerControlCommands
yang test Initializer::execute:addToDocSourceParserMap_listLocalSessions
yang test Initializer::execute:addToExpressionParserMap_ifNull
yang test Initializer::execute:addToDocSourceParserMap_redact
yang test Initializer::execute:addToDocSourceParserMap__internalSplitPipeline
yang test Initializer::execute:addToDocSourceParserMap_graphLookup
yang test Initializer::execute:addToExpressionParserMap_week
yang test Initializer::execute:addToDocSourceParserMap_collStats
yang test Initializer::execute:SetGlobalEnvironment
yang test Initializer::execute:addToExpressionParserMap_pow
yang test Initializer::execute:JavascriptPrintDomain
yang test Initializer::execute:RegisterJournalLatencyTestCmd
yang test Initializer::execute:WiredTigerEngineInit
yang test Initializer::execute:InitializeIndexCatalogFactory
yang test Initializer::execute:addToExpressionParserMap_floor
yang test Initializer::execute:SSLManager
yang test Initializer::execute:CreateReplicationManager
yang test Initializer::execute:EphemeralForTestEngineInit
yang test Initializer::execute:SetupPlanCacheCommands
yang test Initializer::execute:addToDocSourceParserMap_listSessions
yang test Initializer::execute:addToDocSourceParserMap_indexStats
yang test Initializer::execute:InitializeConnectionPools
yang test Initializer::execute:RegisterFaultInjectCmd
yang test Initializer::execute:addToExpressionParserMap_objectToArray
yang test Initializer::execute:RegisterEmptyCappedCmd
yang test Initializer::execute:addToExpressionParserMap_switch
yang test Initializer::execute:InitializeDatabaseHolderFactory
yang test Initializer::execute:InitializeDbHolderimpl
yang test Initializer::execute:periodicNoopIntervalSecs
yang test Initializer::execute:addToAccumulatorFactoryMap_avg
yang test Initializer::execute:addToAccumulatorFactoryMap_stdDevSamp
yang test Initializer::execute:RegisterIsSelfCommand
yang test Initializer::execute:RegisterStageDebugCmd
yang test Initializer::execute:TCMallocThreadIdleListener
yang test Initializer::execute:DevNullEngineInit
yang test Initializer::execute:addToExpressionParserMap_isoWeekYear
yang test Initializer::execute:CreateAuthorizationExternalStateFactory
yang test Initializer::execute:addToExpressionParserMap_dateToParts
yang test Initializer::execute:addToExpressionParserMap_not
yang test Initializer::execute:RegisterCpuLoadCmd
yang test Initializer::execute:InitializeAdvanceClusterTimePrivilegeVector
yang test Initializer::execute:RegisterDbCheckCmd
yang test Initializer::execute:InitializeParseValidationLevelImpl
yang test Initializer::execute:addAliasToDocSourceParserMap_bucket
yang test Initializer::execute:ZlibMessageCompressorInit
yang test Initializer::execute:SetupIndexFilterCommands
yang test Initializer::execute:RegisterAppendOpLogNoteCmd
yang test Initializer::execute:SaslClientAuthenticateFunction
yang test Initializer::execute:SetServerLogContextFunction
yang test Initializer::execute:RegisterReplSetTestCmd
yang test Initializer::execute:NativeSaslServerCore
yang test Initializer::execute:PreSaslCommands
yang test Initializer::execute:addToExpressionParserMap_substrCP
yang test Initializer::execute:addToExpressionParserMap_mergeObjects
yang test Initializer::execute:SetupDottedNames
yang test Initializer::execute:addToExpressionParserMap_trunc
yang test Initializer::execute:addToDocSourceParserMap_lookup
yang test Initializer::execute:addToDocSourceParserMap_facet
yang test Initializer::execute:addToExpressionParserMap_setIsSubset
yang test Initializer::execute:RegisterReapLogicalSessionCacheNowCommand
yang test Initializer::execute:initialSyncOplogBuffer
yang test Initializer::execute:InitializeUserCreateNSImpl
yang test Initializer::execute:InitializeDropAllDatabasesExceptLocalImpl
yang test Initializer::execute:addToDocSourceParserMap_listLocalCursors
yang test Initializer::execute:addToDocSourceParserMap_sample
yang test Initializer::execute:addToDocSourceParserMap_out
yang test Initializer::execute:MungeUmask
yang test Initializer::execute:PostSaslCommands
yang test Initializer::execute:addToExpressionParserMap_isoDayOfWeek
yang test Initializer::execute:addToDocSourceParserMap_mergeCursors
yang test Initializer::execute:addToExpressionParserMap_add
yang test Initializer::execute:addToExpressionParserMap_isArray
yang test Initializer::execute:CreateDiagnosticDataCommand
yang test Initializer::execute:GlobalCursorIdCache
yang test Initializer::execute:GlobalCursorManager
yang test Initializer::execute:RegisterShortCircuitExitHandler
yang test Initializer::execute:addToExpressionParserMap_stdDevPop
yang test Initializer::execute:InitializeCollectionFactory
yang test Initializer::execute:addToExpressionParserMap_minute
yang test Initializer::execute:addToExpressionParserMap_year
yang test Initializer::execute:addToDocSourceParserMap_match
yang test Initializer::execute:InitializeIndexCatalogIndexIteratorFactory
yang test Initializer::execute:InitializePrepareInsertDeleteOptionsImpl
yang test Initializer::execute:InitializeIndexCatalogEntryFactory
yang test Initializer::execute:addToDocSourceParserMap_unwind
yang test Initializer::execute:addToDocSourceParserMap_limit
yang test Initializer::execute:addToGranularityRounderMap_R5
yang test Initializer::execute:addToGranularityRounderMap_R20
yang test Initializer::execute:addToGranularityRounderMap_R40
yang test Initializer::execute:addToGranularityRounderMap_E6
yang test Initializer::execute:addToGranularityRounderMap_E24
yang test Initializer::execute:addToGranularityRounderMap_E48
yang test Initializer::execute:addToGranularityRounderMap_E192
yang test Initializer::execute:addToExpressionParserMap_setDifference
yang test Initializer::execute:addToAccumulatorFactoryMap_addToSet
yang test Initializer::execute:addToExpressionParserMap_avg
yang test Initializer::execute:addToAccumulatorFactoryMap_first
yang test Initializer::execute:addToExpressionParserMap_map
yang test Initializer::execute:addToAccumulatorFactoryMap_last
yang test Initializer::execute:addToAccumulatorFactoryMap_max
yang test Initializer::execute:addToExpressionParserMap_const
yang test Initializer::execute:addToAccumulatorFactoryMap_min
yang test Initializer::execute:addToExpressionParserMap_max
yang test Initializer::execute:addToExpressionParserMap_min
yang test Initializer::execute:addToExpressionParserMap_cond
yang test Initializer::execute:addToAccumulatorFactoryMap_push
yang test Initializer::execute:addToExpressionParserMap_stdDevSamp
yang test Initializer::execute:addToAccumulatorFactoryMap_sum
yang test Initializer::execute:RegisterHashEltCmd
yang test Initializer::execute:addToExpressionParserMap_sum
yang test Initializer::execute:addToAccumulatorFactoryMap_mergeObjects
yang test Initializer::execute:NativeSaslClientContext
yang test Initializer::execute:addToExpressionParserMap_indexOfCP
yang test Initializer::execute:MatchExpressionParser
yang test Initializer::execute:CreateAuthorizationManager
yang test Initializer::execute:addToGranularityRounderMap_1_2_5
yang test Initializer::execute:FTSIndexFormat
yang test Initializer::execute:SnappyMessageCompressorInit
yang test Initializer::execute:AllCompressorsRegistered
yang test Initializer::execute:InitializeParseValidationActionImpl
yang test Initializer::execute:RegisterTopCommand
yang test Initializer::execute:addToExpressionParserMap_ceil
yang test Initializer::execute:addToExpressionParserMap_isoWeek
yang test Initializer::execute:addToExpressionParserMap_divide
yang test Initializer::execute:addToExpressionParserMap_gte
yang test Initializer::execute:CreateCollatorFactory
yang test Initializer::execute:StopWords
yang test Initializer::execute:addToExpressionParserMap_hour
yang test Initializer::execute:addToExpressionParserMap_dayOfYear
yang test Initializer::execute:MMAPV1EngineInit
yang test Initializer::execute:RegisterSnapshotManagementCommands
yang test Initializer::execute:addToExpressionParserMap_setUnion
yang test Initializer::execute:addToDocSourceParserMap_geoNear
yang test Initializer::execute:AuthorizationBuiltinRoles
yang test Initializer::execute:ModifierTable
yang test Initializer::execute:addToGranularityRounderMap_E12
yang test Initializer::execute:addToExpressionParserMap_literal
yang test Initializer::execute:PathlessOperatorMap
yang test Initializer::execute:addToExpressionParserMap_strLenCP
yang test Initializer::execute:addToExpressionParserMap_dayOfMonth
yang test Initializer::execute:InitializeDatabaseFactory
yang test Initializer::execute:addToExpressionParserMap_millisecond
yang test Initializer::execute:addToExpressionParserMap_dayOfWeek
yang test Initializer::execute:addToExpressionParserMap_second
yang test Initializer::execute:SecureAllocator
yang test Initializer::execute:addToExpressionParserMap_abs
yang test Initializer::execute:addToGranularityRounderMap_POWERSOF2
yang test Initializer::execute:addToExpressionParserMap_anyElementTrue
yang test Initializer::execute:addToExpressionParserMap_size
yang test Initializer::execute:addToExpressionParserMap_arrayElemAt
yang test Initializer::execute:S2CellIdInit
yang test Initializer::execute:addToExpressionParserMap_arrayToObject
yang test Initializer::execute:addToExpressionParserMap_setIntersection
yang test Initializer::execute:addToExpressionParserMap_cmp
yang test Initializer::execute:addToExpressionParserMap_eq
yang test Initializer::execute:addToExpressionParserMap_gt
yang test Initializer::execute:addToExpressionParserMap_ne
yang test Initializer::execute:addToExpressionParserMap_concatArrays
yang test Initializer::execute:addToExpressionParserMap_concat
yang test Initializer::execute:addToExpressionParserMap_dateFromParts
yang test Initializer::execute:addToExpressionParserMap_dateToString
yang test Initializer::execute:addToExpressionParserMap_exp
yang test Initializer::execute:addToExpressionParserMap_let
yang test Initializer::execute:addToExpressionParserMap_in
yang test Initializer::execute:addToExpressionParserMap_indexOfArray
yang test Initializer::execute:addToExpressionParserMap_ln
yang test Initializer::execute:addToExpressionParserMap_log
yang test Initializer::execute:addToExpressionParserMap_log10
yang test Initializer::execute:addToExpressionParserMap_range
yang test Initializer::execute:addToExpressionParserMap_reduce
yang test Initializer::execute:addToExpressionParserMap_reverseArray
yang test Initializer::execute:addToExpressionParserMap_setEquals
yang test Initializer::execute:addToExpressionParserMap_slice
yang test Initializer::execute:addToDocSourceParserMap_bucketAuto
yang test Initializer::execute:LoadTimeZoneDB
yang test Initializer::execute:addToExpressionParserMap_split
yang test Initializer::execute:addToExpressionParserMap_sqrt
yang test Initializer::execute:addToGranularityRounderMap_R80
yang test Initializer::execute:addToExpressionParserMap_strcasecmp
yang test Initializer::execute:addToExpressionParserMap_substrBytes
yang test Initializer::execute:addToExpressionParserMap_strLenBytes
yang test Initializer::execute:addToExpressionParserMap_subtract
yang test Initializer::execute:addToExpressionParserMap_type
yang test Initializer::execute:addToExpressionParserMap_zip
yang test Initializer::execute:SetWiredTigerExtensions
yang test Initializer::execute:S2RegionCovererInit
yang test Initializer::execute:replSettingsCheck
yang test Initializer::execute:SetWiredTigerCustomizationHooks
yang test Initializer::execute:InitializeDropDatabaseImpl
yang test Initializer::execute:SetEncryptionHooks
*/
//args命令行参数，env系统环境变量信息  runGlobalInitializers 中调用执行
Status Initializer::execute(const InitializerContext::ArgumentVector& args,
                            const InitializerContext::EnvironmentMap& env) const {
    std::vector<std::string> sortedNodes;
    Status status = _graph.topSort(&sortedNodes);
    if (Status::OK() != status)
        return status;
	
    InitializerContext context(args, env);

    for (size_t i = 0; i < sortedNodes.size(); ++i) {
		/*
		std::cout << "yang test Initializer::execute:" << sortedNodes[i] << std::endl;
        */
        InitializerFunction fn = _graph.getInitializerFunction(sortedNodes[i]);
        if (!fn) {
            return Status(ErrorCodes::InternalError,
                          "topSort returned a node that has no associated function: \"" +
                              sortedNodes[i] + '"');
        }
        try {
            status = fn(&context);
        } catch (const DBException& xcp) {
            return xcp.toStatus();
        }

        if (Status::OK() != status)
            return status;
    }
    return Status::OK();
}

Status runGlobalInitializers(const InitializerContext::ArgumentVector& args,
                             const InitializerContext::EnvironmentMap& env) {
    return getGlobalInitializer().execute(args, env);
}

Status runGlobalInitializers(int argc, const char* const* argv, const char* const* envp) {
    InitializerContext::ArgumentVector args(argc);
    std::copy(argv, argv + argc, args.begin()); //拷贝命令行参数到args数组

    InitializerContext::EnvironmentMap env;

    if (envp) { //envp存储的是export设置的环境变量信息存入env map表中
        for (; *envp; ++envp) {
            const char* firstEqualSign = strchr(*envp, '=');
            if (!firstEqualSign) {
                return Status(ErrorCodes::BadValue, "malformed environment block");
            }
            env[std::string(*envp, firstEqualSign)] = std::string(firstEqualSign + 1);
			//std::cout << "yang test runGlobalInitializers:" << env[std::string(*envp, firstEqualSign)] << ":" << std::string(firstEqualSign + 1) << std::endl;
        }
    }

    return runGlobalInitializers(args, env);
}

void runGlobalInitializersOrDie(int argc, const char* const* argv, const char* const* envp) {
    Status status = runGlobalInitializers(argc, argv, envp);
    if (!status.isOK()) {
        std::cerr << "Failed global initialization: " << status << std::endl;
        quickExit(1);
    }
}

}  // namespace mongo
