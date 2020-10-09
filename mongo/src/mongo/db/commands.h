/**
 *    Copyright (C) 2009-2016 MongoDB Inc.
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

#include <boost/optional.hpp>
#include <string>
#include <vector>

#include "mongo/base/counter.h"
#include "mongo/base/status.h"
#include "mongo/base/status_with.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/auth/resource_pattern.h"
#include "mongo/db/client.h"
#include "mongo/db/commands/server_status_metric.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/query/explain.h"
#include "mongo/db/write_concern.h"
#include "mongo/rpc/reply_builder_interface.h"
#include "mongo/stdx/functional.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/string_map.h"

namespace mongo {

class OperationContext;
class Timer;

namespace mutablebson {
class Document;
}  // namespace mutablebson

class CommandInterface {
protected:
    CommandInterface() = default;

public:
    virtual ~CommandInterface() = default;

    /**
     * Returns the command's name. This value never changes for the lifetime of the command.
     */
    virtual const std::string& getName() const = 0;

    /**
     * Return the namespace for the command. If the first field in 'cmdObj' is of type
     * mongo::String, then that field is interpreted as the collection name, and is
     * appended to 'dbname' after a '.' character. If the first field is not of type
     * mongo::String, then 'dbname' is returned unmodified.
     */
    virtual std::string parseNs(const std::string& dbname, const BSONObj& cmdObj) const = 0;

    /**
     * Utility that returns a ResourcePattern for the namespace returned from
     * parseNs(dbname, cmdObj).  This will be either an exact namespace resource pattern
     * or a database resource pattern, depending on whether parseNs returns a fully qualifed
     * collection name or just a database name.
     */
    virtual ResourcePattern parseResourcePattern(const std::string& dbname,
                                                 const BSONObj& cmdObj) const = 0;

    /**
     * Used by command implementations to hint to the rpc system how much space they will need in
     * their replies.
     */
    virtual std::size_t reserveBytesForReply() const = 0;

    /**
     * supportsWriteConcern returns true if this command should be parsed for a writeConcern
     * field and wait for that write concern to be satisfied after the command runs.
     *
     * @param cmd is a BSONObj representation of the command that is used to determine if the
     *            the command supports a write concern. Ex. aggregate only supports write concern
     *            when $out is provided.
     */
    virtual bool supportsWriteConcern(const BSONObj& cmd) const = 0;

    /**
     * Return true if only the admin ns has privileges to run this command.
     */
    virtual bool adminOnly() const = 0;

    /**
     * Like adminOnly, but even stricter: we must either be authenticated for admin db,
     * or, if running without auth, on the local interface.  Used for things which
     * are so major that remote invocation may not make sense (e.g., shutdownServer).
     *
     * When localHostOnlyIfNoAuth() is true, adminOnly() must also be true.
     */
    virtual bool localHostOnlyIfNoAuth() = 0;

    /* Return true if slaves are allowed to execute the command
    */
    virtual bool slaveOk() const = 0;

    /**
     * Return true if the client force a command to be run on a slave by
     * turning on the 'slaveOk' option in the command query.
     */
    virtual bool slaveOverrideOk() const = 0;

    /**
     * Override and return fales if the command opcounters should not be incremented on
     * behalf of this command.
     */
    virtual bool shouldAffectCommandCounter() const = 0;

    /**
     * Return true if the command requires auth.
    */
    virtual bool requiresAuth() const = 0;

    /**
     * Generates help text for this command.
     */
    virtual void help(std::stringstream& help) const = 0;

    /**
     * Commands which can be explained override this method. Any operation which has a query
     * part and executes as a tree of execution stages can be explained. A command should
     * implement explain by:
     *
     *   1) Calling its custom parse function in order to parse the command. The output of
     *   this function should be a CanonicalQuery (representing the query part of the
     *   operation), and a PlanExecutor which wraps the tree of execution stages.
     *
     *   2) Calling Explain::explainStages(...) on the PlanExecutor. This is the function
     *   which knows how to convert an execution stage tree into explain output.
     */
    virtual Status explain(OperationContext* opCtx,
                           const std::string& dbname,
                           const BSONObj& cmdObj,
                           ExplainOptions::Verbosity verbosity,
                           BSONObjBuilder* out) const = 0;

    /**
     * Checks if the client associated with the given OperationContext is authorized to run this
     * command.
     */
    virtual Status checkAuthForRequest(OperationContext* opCtx, const OpMsgRequest& request) = 0;

    /**
     * Redacts "cmdObj" in-place to a form suitable for writing to logs.
     *
     * The default implementation does nothing.
     */
    virtual void redactForLogging(mutablebson::Document* cmdObj) = 0;

    /**
     * Returns a copy of "cmdObj" in a form suitable for writing to logs.
     * Uses redactForLogging() to transform "cmdObj".
     */
    virtual BSONObj getRedactedCopyForLogging(const BSONObj& cmdObj) = 0;

    /**
     * Return true if a replica set secondary should go into "recovering"
     * (unreadable) state while running this command.
     */
    virtual bool maintenanceMode() const = 0;

    /**
     * Return true if command should be permitted when a replica set secondary is in "recovering"
     * (unreadable) state.
     */
    virtual bool maintenanceOk() const = 0;

    /**
     * Returns true if this Command supports the non-local readConcern:level field value. Takes the
     * command object and the name of the database on which it was invoked as arguments, so that
     * readConcern can be conditionally rejected based on the command's parameters and/or namespace.
     *
     * If the readConcern non-local level argument is sent to a command that returns false the
     * command processor will reject the command, returning an appropriate error message. For
     * commands that support the argument, the command processor will instruct the RecoveryUnit to
     * only return "committed" data, failing if this isn't supported by the storage engine.
     *
     * Note that this is never called on mongos. Sharded commands are responsible for forwarding
     * the option to the shards as needed. We rely on the shards to fail the commands in the
     * cases where it isn't supported.
     */
    virtual bool supportsNonLocalReadConcern(const std::string& dbName,
                                             const BSONObj& cmdObj) const = 0;

    /**
     * Returns true if command allows afterClusterTime in its readConcern. The command may not allow
     * it if it is specifically intended not to take any LockManager locks. Waiting for
     * afterClusterTime takes the MODE_IS lock.
     */
    virtual bool allowsAfterClusterTime(const BSONObj& cmdObj) const = 0;

    /**
     * Returns LogicalOp for this command.
     */
    virtual LogicalOp getLogicalOp() const = 0;

    /**
     * Returns whether this operation is a read, write, or command.
     *
     * Commands which implement database read or write logic should override this to return kRead
     * or kWrite as appropriate.
     */
    enum class ReadWriteType { kCommand, kRead, kWrite };
    virtual ReadWriteType getReadWriteType() const = 0;

    /**
     * Increment counter for how many times this command has executed.
     */
    virtual void incrementCommandsExecuted() = 0;

    /**
     * Increment counter for how many times this command has failed.
     */
    virtual void incrementCommandsFailed() = 0;
};

/**
 * Serves as a base for server commands. See the constructor for more details.
 */ 

/* mongo命令
[root@bogon mongo]# grep "public BasicCommand {" * -r
db/auth/sasl_commands.cpp:class CmdSaslStart : public BasicCommand {
db/auth/sasl_commands.cpp:class CmdSaslContinue : public BasicCommand {
db/commands.h:[root@bogon s]# grep "public BasicCommand {" * -r
db/commands.h:client/shard_connection.cpp:class ShardedPoolStats : public BasicCommand {
db/commands.h:commands/cluster_add_shard_cmd.cpp:class AddShardCmd : public BasicCommand {
db/commands.h:commands/cluster_add_shard_to_zone_cmd.cpp:class AddShardToZoneCmd : public BasicCommand {
db/commands.h:commands/cluster_available_query_options_cmd.cpp:class AvailableQueryOptions : public BasicCommand {
db/commands.h:commands/cluster_compact_cmd.cpp:class CompactCmd : public BasicCommand {
db/commands.h:commands/cluster_control_balancer_cmd.cpp:class BalancerControlCommand : public BasicCommand {
db/commands.h:commands/cluster_drop_cmd.cpp:class DropCmd : public BasicCommand {
db/commands.h:commands/cluster_drop_database_cmd.cpp:class DropDatabaseCmd : public BasicCommand {
db/commands.h:commands/cluster_explain_cmd.cpp:class ClusterExplainCmd : public BasicCommand {
db/commands.h:commands/cluster_find_and_modify_cmd.cpp:class FindAndModifyCmd : public BasicCommand {
db/commands.h:commands/cluster_find_cmd.cpp:class ClusterFindCmd : public BasicCommand {
db/commands.h:commands/cluster_flush_router_config_cmd.cpp:class FlushRouterConfigCmd : public BasicCommand {
db/commands.h:commands/cluster_get_last_error_cmd.cpp:class GetLastErrorCmd : public BasicCommand {
db/commands.h:commands/cluster_get_shard_map_cmd.cpp:class CmdGetShardMap : public BasicCommand {
db/commands.h:commands/cluster_get_shard_version_cmd.cpp:class GetShardVersion : public BasicCommand {
db/commands.h:commands/cluster_getmore_cmd.cpp://class ClusterGetMoreCmd final : public BasicCommand { yang add change
db/commands.h:commands/cluster_getmore_cmd.cpp:class ClusterGetMoreCmd : public BasicCommand {
db/commands.h:commands/cluster_index_filter_cmd.cpp:class ClusterIndexFilterCmd : public BasicCommand {
db/commands.h:commands/cluster_is_db_grid_cmd.cpp:class IsDbGridCmd : public BasicCommand {
db/commands.h:commands/cluster_is_master_cmd.cpp:class CmdIsMaster : public BasicCommand {
db/commands.h:commands/cluster_kill_op.cpp:class ClusterKillOpCommand : public BasicCommand {
db/commands.h:commands/cluster_list_databases_cmd.cpp:class ListDatabasesCmd : public BasicCommand {
db/commands.h:commands/cluster_list_shards_cmd.cpp:class ListShardsCmd : public BasicCommand {
db/commands.h:commands/cluster_move_primary_cmd.cpp:class MoveDatabasePrimaryCommand : public BasicCommand {
db/commands.h:commands/cluster_multicast.cpp:class MulticastCmd : public BasicCommand {
db/commands.h:commands/cluster_netstat_cmd.cpp:class NetStatCmd : public BasicCommand {
db/commands.h:commands/cluster_pipeline_cmd.cpp:class ClusterPipelineCommand : public BasicCommand {
db/commands.h:commands/cluster_plan_cache_cmd.cpp:class ClusterPlanCacheCmd : public BasicCommand {
db/commands.h:commands/cluster_remove_shard_cmd.cpp:class RemoveShardCmd : public BasicCommand {
db/commands.h:commands/cluster_remove_shard_from_zone_cmd.cpp:class RemoveShardFromZoneCmd : public BasicCommand {
db/commands.h:commands/cluster_reset_error_cmd.cpp:class CmdShardingResetError : public BasicCommand {
db/commands.h:commands/cluster_set_feature_compatibility_version_cmd.cpp:class SetFeatureCompatibilityVersionCmd : public BasicCommand {
db/commands.h:commands/cluster_shard_collection_cmd.cpp:class ShardCollectionCmd : public BasicCommand {
db/commands.h:commands/cluster_update_zone_key_range_cmd.cpp:class UpdateZoneKeyRangeCmd : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdCreateUser : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdUpdateUser : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdDropUser : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdDropAllUsersFromDatabase : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdGrantRolesToUser : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdRevokeRolesFromUser : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdUsersInfo : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdCreateRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdUpdateRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdGrantPrivilegesToRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdRevokePrivilegesFromRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdGrantRolesToRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdRevokeRolesFromRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdDropRole : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdDropAllRolesFromDatabase : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdRolesInfo : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdInvalidateUserCache : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdMergeAuthzCollections : public BasicCommand {
db/commands.h:commands/cluster_user_management_commands.cpp:class CmdAuthSchemaUpgrade : public BasicCommand {
db/commands.h:commands/cluster_whats_my_uri_cmd.cpp:class WhatsMyUriCmd : public BasicCommand {
db/commands.h:commands/commands_public.cpp:class PublicGridCommand : public BasicCommand {
db/commands.h:class ErrmsgCommandDeprecated : public BasicCommand {
db/commands/authentication_commands.cpp:class CmdGetNonce : public BasicCommand {
db/commands/authentication_commands.cpp:class CmdLogout : public BasicCommand {
db/commands/authentication_commands.h:class CmdAuthenticate : public BasicCommand {
db/commands/clone.cpp:class CmdClone : public BasicCommand {
db/commands/conn_pool_stats.cpp:class PoolStats final : public BasicCommand {
db/commands/conn_pool_sync.cpp:class PoolFlushCmd : public BasicCommand {
db/commands/connection_status.cpp:class CmdConnectionStatus : public BasicCommand {
db/commands/count_cmd.cpp:class CmdCount : public BasicCommand {
db/commands/cpuload.cpp:class CPULoadCommand : public BasicCommand {
db/commands/current_op_common.h:class CurrentOpCommandBase : public BasicCommand {
db/commands/dbcheck.cpp:class DbCheckCmd : public BasicCommand {
db/commands/dbcommands.cpp:class CmdDropDatabase : public BasicCommand {
db/commands/dbcommands.cpp:class CmdCreate : public BasicCommand {
db/commands/dbcommands.cpp:class CmdFileMD5 : public BasicCommand {
db/commands/dbcommands.cpp:class CollectionModCommand : public BasicCommand {
db/commands/dbcommands.cpp:class CmdWhatsMyUri : public BasicCommand {
db/commands/dbcommands.cpp:class AvailableQueryOptions : public BasicCommand {
db/commands/distinct.cpp:class DistinctCommand : public BasicCommand {
db/commands/drop_indexes.cpp:class CmdDropIndexes : public BasicCommand {
db/commands/end_sessions_command.cpp:class EndSessionsCommand final : public BasicCommand {
db/commands/explain_cmd.cpp:class CmdExplain : public BasicCommand {
db/commands/find_and_modify.cpp:class CmdFindAndModify : public BasicCommand {
db/commands/find_cmd.cpp:class FindCmd : public BasicCommand {
db/commands/generic.cpp:class CmdBuildInfo : public BasicCommand {
db/commands/generic.cpp:class PingCommand : public BasicCommand {
db/commands/generic.cpp:class FeaturesCmd : public BasicCommand {
db/commands/generic.cpp:class HostInfoCmd : public BasicCommand {
db/commands/generic.cpp:class LogRotateCmd : public BasicCommand {
db/commands/generic.cpp:class ListCommandsCmd : public BasicCommand {
db/commands/generic.cpp:class CmdForceError : public BasicCommand {
db/commands/generic.cpp:class ClearLogCmd : public BasicCommand {
db/commands/generic.cpp:class CmdGetCmdLineOpts : public BasicCommand {
db/commands/get_last_error.cpp:class CmdResetError : public BasicCommand {
db/commands/get_last_error.cpp:class CmdGetPrevError : public BasicCommand {
db/commands/getmore_cmd.cpp:class GetMoreCmd : public BasicCommand {
db/commands/group_cmd.cpp:class GroupCommand : public BasicCommand {
db/commands/index_filter_commands.h:class IndexFilterCommand : public BasicCommand {
db/commands/isself.cpp:class IsSelfCommand : public BasicCommand {
db/commands/kill_all_sessions_by_pattern_command.cpp:class KillAllSessionsByPatternCommand final : public BasicCommand {
db/commands/kill_all_sessions_command.cpp:class KillAllSessionsCommand final : public BasicCommand {
db/commands/kill_op.cpp:class KillOpCommand : public BasicCommand {
db/commands/kill_sessions_command.cpp:class KillSessionsCommand final : public BasicCommand {
db/commands/killcursors_common.h:class KillCursorsCmdBase : public BasicCommand {
db/commands/list_collections.cpp:class CmdListCollections : public BasicCommand {
db/commands/list_databases.cpp:class CmdListDatabases : public BasicCommand {
db/commands/list_indexes.cpp:class CmdListIndexes : public BasicCommand {
db/commands/lock_info.cpp:class CmdLockInfo : public BasicCommand {
db/commands/mr.cpp:class MapReduceFinishCommand : public BasicCommand {
db/commands/oplog_note.cpp:class AppendOplogNoteCmd : public BasicCommand {
db/commands/parallel_collection_scan.cpp:class ParallelCollectionScanCmd : public BasicCommand {
db/commands/pipeline_command.cpp:class PipelineCommand : public BasicCommand {
db/commands/plan_cache_commands.h:class PlanCacheCommand : public BasicCommand {
db/commands/reap_logical_session_cache_now.cpp:class ReapLogicalSessionCacheNowCommand final : public BasicCommand {
db/commands/refresh_logical_session_cache_now.cpp:class RefreshLogicalSessionCacheNowCommand final : public BasicCommand {
db/commands/refresh_sessions_command.cpp:class RefreshSessionsCommand final : public BasicCommand {
db/commands/refresh_sessions_command_internal.cpp:class RefreshSessionsCommandInternal final : public BasicCommand {
db/commands/repair_cursor.cpp:class RepairCursorCmd : public BasicCommand {
db/commands/resize_oplog.cpp:class CmdReplSetResizeOplog : public BasicCommand {
db/commands/server_status.cpp:class CmdServerStatus : public BasicCommand {
db/commands/set_feature_compatibility_version_command.cpp:class SetFeatureCompatibilityVersionCommand : public BasicCommand {
db/commands/shutdown.h:class CmdShutdown : public BasicCommand {
db/commands/snapshot_management.cpp:class CmdMakeSnapshot final : public BasicCommand {
db/commands/snapshot_management.cpp:class CmdSetCommittedSnapshot final : public BasicCommand {
db/commands/start_session_command.cpp:class StartSessionCommand final : public BasicCommand {
db/commands/test_commands.cpp:class CmdSleep : public BasicCommand {
db/commands/test_commands.cpp:class CapTrunc : public BasicCommand {
db/commands/test_commands.cpp:class EmptyCapped : public BasicCommand {
db/commands/top_command.cpp:class TopCommand : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdCreateUser : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdUpdateUser : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdDropUser : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdDropAllUsersFromDatabase : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdGrantRolesToUser : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdRevokeRolesFromUser : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdUsersInfo : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdCreateRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdUpdateRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdGrantPrivilegesToRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdRevokePrivilegesFromRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdGrantRolesToRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdRevokeRolesFromRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdDropRole : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdDropAllRolesFromDatabase : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdRolesInfo : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdInvalidateUserCache : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdGetCacheGeneration : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdMergeAuthzCollections : public BasicCommand {
db/commands/user_management_commands.cpp:class CmdAuthSchemaUpgrade : public BasicCommand {
db/commands/validate.cpp:class ValidateCmd : public BasicCommand {
db/exec/stagedebug_cmd.cpp:class StageDebugCmd : public BasicCommand {
db/ftdc/ftdc_commands.cpp:class GetDiagnosticDataCommand final : public BasicCommand {
db/repl/master_slave.cpp:class HandshakeCmd : public BasicCommand {
db/repl/repl_set_command.h:class ReplSetCommand : public BasicCommand {
db/repl/replication_info.cpp:class CmdIsMaster : public BasicCommand {
db/s/config/configsvr_add_shard_command.cpp:class ConfigSvrAddShardCommand : public BasicCommand {
db/s/config/configsvr_add_shard_to_zone_command.cpp:class ConfigSvrAddShardToZoneCommand : public BasicCommand {
db/s/config/configsvr_commit_chunk_migration_command.cpp:class ConfigSvrCommitChunkMigrationCommand : public BasicCommand {
db/s/config/configsvr_control_balancer_command.cpp:class ConfigSvrBalancerControlCommand : public BasicCommand {
db/s/config/configsvr_create_database_command.cpp:class ConfigSvrCreateDatabaseCommand : public BasicCommand {
db/s/config/configsvr_enable_sharding_command.cpp:class ConfigSvrEnableShardingCommand : public BasicCommand {
db/s/config/configsvr_merge_chunk_command.cpp:class ConfigSvrMergeChunkCommand : public BasicCommand {
db/s/config/configsvr_move_chunk_command.cpp:class ConfigSvrMoveChunkCommand : public BasicCommand {
db/s/config/configsvr_move_primary_command.cpp:class ConfigSvrMovePrimaryCommand : public BasicCommand {
db/s/config/configsvr_remove_shard_command.cpp:class ConfigSvrRemoveShardCommand : public BasicCommand {
db/s/config/configsvr_remove_shard_from_zone_command.cpp:class ConfigSvrRemoveShardFromZoneCommand : public BasicCommand {
db/s/config/configsvr_shard_collection_command.cpp:class ConfigSvrShardCollectionCommand : public BasicCommand {
db/s/config/configsvr_split_chunk_command.cpp:class ConfigSvrSplitChunkCommand : public BasicCommand {
db/s/config/configsvr_update_zone_key_range_command.cpp:class ConfigsvrUpdateZoneKeyRangeCommand : public BasicCommand {
db/s/flush_routing_table_cache_updates_command.cpp:class FlushRoutingTableCacheUpdates : public BasicCommand {
db/s/get_shard_version_command.cpp:class GetShardVersion : public BasicCommand {
db/s/migration_chunk_cloner_source_legacy_commands.cpp:class InitialCloneCommand : public BasicCommand {
db/s/migration_chunk_cloner_source_legacy_commands.cpp:class TransferModsCommand : public BasicCommand {
db/s/migration_chunk_cloner_source_legacy_commands.cpp:class MigrateSessionCommand : public BasicCommand {
db/s/migration_destination_manager_legacy_commands.cpp:class RecvChunkStatusCommand : public BasicCommand {
db/s/migration_destination_manager_legacy_commands.cpp:class RecvChunkCommitCommand : public BasicCommand {
db/s/migration_destination_manager_legacy_commands.cpp:class RecvChunkAbortCommand : public BasicCommand {
db/s/move_chunk_command.cpp:class MoveChunkCommand : public BasicCommand {
db/s/sharding_state_command.cpp:class ShardingStateCmd : public BasicCommand {
db/s/unset_sharding_command.cpp:class UnsetShardingCommand : public BasicCommand {
db/storage/mmap_v1/journal_latency_test_cmd.cpp:class JournalLatencyTestCmd : public BasicCommand {
s/client/shard_connection.cpp:class ShardedPoolStats : public BasicCommand {
s/commands/cluster_add_shard_cmd.cpp:class AddShardCmd : public BasicCommand {
s/commands/cluster_add_shard_to_zone_cmd.cpp:class AddShardToZoneCmd : public BasicCommand {
s/commands/cluster_available_query_options_cmd.cpp:class AvailableQueryOptions : public BasicCommand {
s/commands/cluster_compact_cmd.cpp:class CompactCmd : public BasicCommand {
s/commands/cluster_control_balancer_cmd.cpp:class BalancerControlCommand : public BasicCommand {
s/commands/cluster_drop_cmd.cpp:class DropCmd : public BasicCommand {
s/commands/cluster_drop_database_cmd.cpp:class DropDatabaseCmd : public BasicCommand {
s/commands/cluster_explain_cmd.cpp:class ClusterExplainCmd : public BasicCommand {
s/commands/cluster_find_and_modify_cmd.cpp:class FindAndModifyCmd : public BasicCommand {
s/commands/cluster_find_cmd.cpp:class ClusterFindCmd : public BasicCommand {
s/commands/cluster_flush_router_config_cmd.cpp:class FlushRouterConfigCmd : public BasicCommand {
s/commands/cluster_get_last_error_cmd.cpp:class GetLastErrorCmd : public BasicCommand {
s/commands/cluster_get_shard_map_cmd.cpp:class CmdGetShardMap : public BasicCommand {
s/commands/cluster_get_shard_version_cmd.cpp:class GetShardVersion : public BasicCommand {
s/commands/cluster_getmore_cmd.cpp://class ClusterGetMoreCmd final : public BasicCommand { yang add change
s/commands/cluster_getmore_cmd.cpp:class ClusterGetMoreCmd : public BasicCommand {
s/commands/cluster_index_filter_cmd.cpp:class ClusterIndexFilterCmd : public BasicCommand {
s/commands/cluster_is_db_grid_cmd.cpp:class IsDbGridCmd : public BasicCommand {
s/commands/cluster_is_master_cmd.cpp:class CmdIsMaster : public BasicCommand {
s/commands/cluster_kill_op.cpp:class ClusterKillOpCommand : public BasicCommand {
s/commands/cluster_list_databases_cmd.cpp:class ListDatabasesCmd : public BasicCommand {
s/commands/cluster_list_shards_cmd.cpp:class ListShardsCmd : public BasicCommand {
s/commands/cluster_move_primary_cmd.cpp:class MoveDatabasePrimaryCommand : public BasicCommand {
s/commands/cluster_multicast.cpp:class MulticastCmd : public BasicCommand {
s/commands/cluster_netstat_cmd.cpp:class NetStatCmd : public BasicCommand {
s/commands/cluster_pipeline_cmd.cpp:class ClusterPipelineCommand : public BasicCommand {
s/commands/cluster_plan_cache_cmd.cpp:class ClusterPlanCacheCmd : public BasicCommand {
s/commands/cluster_remove_shard_cmd.cpp:class RemoveShardCmd : public BasicCommand {
s/commands/cluster_remove_shard_from_zone_cmd.cpp:class RemoveShardFromZoneCmd : public BasicCommand {
s/commands/cluster_reset_error_cmd.cpp:class CmdShardingResetError : public BasicCommand {
s/commands/cluster_set_feature_compatibility_version_cmd.cpp:class SetFeatureCompatibilityVersionCmd : public BasicCommand {
s/commands/cluster_shard_collection_cmd.cpp:class ShardCollectionCmd : public BasicCommand {
s/commands/cluster_update_zone_key_range_cmd.cpp:class UpdateZoneKeyRangeCmd : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdCreateUser : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdUpdateUser : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdDropUser : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdDropAllUsersFromDatabase : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdGrantRolesToUser : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdRevokeRolesFromUser : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdUsersInfo : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdCreateRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdUpdateRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdGrantPrivilegesToRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdRevokePrivilegesFromRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdGrantRolesToRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdRevokeRolesFromRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdDropRole : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdDropAllRolesFromDatabase : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdRolesInfo : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdInvalidateUserCache : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdMergeAuthzCollections : public BasicCommand {
s/commands/cluster_user_management_commands.cpp:class CmdAuthSchemaUpgrade : public BasicCommand {
s/commands/cluster_whats_my_uri_cmd.cpp:class WhatsMyUriCmd : public BasicCommand {
s/commands/commands_public.cpp:class PublicGridCommand : public BasicCommand {
[root@bogon mongo]# 
[root@bogon mongo]# 
[root@bogon mongo]# grep "public Command {" * -r
db/commands.h:[root@bogon s]# grep "public Command {" * -r
db/commands.h:commands/cluster_write_cmd.cpp:class ClusterWriteCmd : public Command {
db/commands.h:class BasicCommand : public Command {
db/commands/write_commands/write_commands.cpp:class WriteCommand : public Command {
s/commands/cluster_write_cmd.cpp:class ClusterWriteCmd : public Command {
[root@bogon mongo]# 
*/

/*
命令注册可以参考
src/mongo/db/auth/action_types.txt:"enableProfiler",
src/mongo/db/auth/role_graph_builtin_roles.cpp:        << ActionType::enableProfiler
src/mongo/db/commands/dbcommands.cpp:                                                           ActionType::enableProfiler)) {
src/mongo/s/commands/cluster_profile_cmd.cpp:        actions.addAction(ActionType::enableProfiler);
*/

//BasicCommand继承该类(类构造一般由basicCommand实现，如AddShardCmd()等) ClusterWriteCmd(mongos) WriteCommand(mongod)  Command类来源见Command::findCommand
//mongod  WriteCommand(CmdInsert  CmdUpdate  CmdDelete等继承WriteCommand类,WriteCommand继承Command类)
//mongos  ClusterWriteCmd(ClusterCmdInsert  ClusterCmdUpdate  ClusterCmdDelete类继承该类，对应mongos转发)

//mongos和mongod支持的命令统计都不一样，通过 db.serverStatus().metrics.commands查看命令统计信息
//Command::findCommand中通过c = Command::findCommand(request.getCommandName())获取对应的command

//ClusterWriteCmd  WriteCommand  BasicCommand继承该类   
class Command : public CommandInterface {
public:
    // The type of the first field in 'cmdObj' must be mongo::String. The first field is
    // interpreted as a collection name.
    static std::string parseNsFullyQualified(const std::string& dbname, const BSONObj& cmdObj);

    // The type of the first field in 'cmdObj' must be mongo::String or Symbol.
    // The first field is interpreted as a collection name.
    static NamespaceString parseNsCollectionRequired(const std::string& dbname,
                                                     const BSONObj& cmdObj);
    static NamespaceString parseNsOrUUID(OperationContext* opCtx,
                                         const std::string& dbname,
                                         const BSONObj& cmdObj);

    using CommandMap = StringMap<Command*>;

    /**
     * Constructs a new command and causes it to be registered with the global commands list. It is
     * not safe to construct commands other than when the server is starting up.
     *
     * @param oldName an optional old, deprecated name for the command
     */
    Command(StringData name, StringData oldName = StringData());

    // NOTE: Do not remove this declaration, or relocate it in this class. We
    // are using this method to control where the vtable is emitted.
    virtual ~Command();

    const std::string& getName() const final {
        return _name;
    }

    std::string parseNs(const std::string& dbname, const BSONObj& cmdObj) const override;

    ResourcePattern parseResourcePattern(const std::string& dbname,
                                         const BSONObj& cmdObj) const override;

    std::size_t reserveBytesForReply() const override {
        return 0u;
    }
    //该命令只能在admin中执行
    bool adminOnly() const override {
        return false;
    }

    bool localHostOnlyIfNoAuth() override {
        return false;
    }

    bool slaveOverrideOk() const override {
        return false;
    }

    bool shouldAffectCommandCounter() const override {
        return true;
    }

    bool requiresAuth() const override {
        return true;
    }

    void help(std::stringstream& help) const override;

    Status explain(OperationContext* opCtx,
                   const std::string& dbname,
                   const BSONObj& cmdObj,
                   ExplainOptions::Verbosity verbosity,
                   BSONObjBuilder* out) const override;

    void redactForLogging(mutablebson::Document* cmdObj) override;

    BSONObj getRedactedCopyForLogging(const BSONObj& cmdObj) override;

    bool maintenanceMode() const override {
        return false;
    }

    bool maintenanceOk() const override {
        return true; /* assumed true prior to commit */
    }

    bool supportsNonLocalReadConcern(const std::string& dbName,
                                     const BSONObj& cmdObj) const override {
        return false;
    }

    bool allowsAfterClusterTime(const BSONObj& cmdObj) const override {
        return true;
    }

    LogicalOp getLogicalOp() const override {
        return LogicalOp::opCommand;
    }

    ReadWriteType getReadWriteType() const override {
        return ReadWriteType::kCommand;
    }

    void incrementCommandsExecuted() final {
        _commandsExecuted.increment();
    }

    void incrementCommandsFailed() final {
        _commandsFailed.increment();
    }

    /**
     * Runs the command.
     *
     * Forwards to enhancedRun, but additionally runs audit checks if run throws unauthorized.
     */
    bool publicRun(OperationContext* opCtx, const OpMsgRequest& request, BSONObjBuilder& result);

    static const CommandMap& allCommands() {
        return *_commands;
    }

    static const CommandMap& allCommandsByBestName() {
        return *_commandsByBestName;
    }

    // Counter for unknown commands
    static Counter64 unknownCommands;

    /**
     * Runs a command directly and returns the result. Does not do any other work normally handled
     * by command dispatch, such as checking auth, dealing with CurOp or waiting for write concern.
     * It is illegal to call this if the command does not exist.
     */
    static BSONObj runCommandDirectly(OperationContext* txn, const OpMsgRequest& request);

    static Command* findCommand(StringData name);

    // Helper for setting errmsg and ok field in command result object.
    static void appendCommandStatus(BSONObjBuilder& result,
                                    bool ok,
                                    const std::string& errmsg = {});

    // @return s.isOK()
    static bool appendCommandStatus(BSONObjBuilder& result, const Status& status);

    /**
     * Helper for setting a writeConcernError field in the command result object if
     * a writeConcern error occurs.
     *
     * @param result is the BSONObjBuilder for the command response. This function creates the
     *               writeConcernError field for the response.
     * @param awaitReplicationStatus is the status received from awaitReplication.
     * @param wcResult is the writeConcernResult object that holds other write concern information.
     *      This is primarily used for populating errInfo when a timeout occurs, and is populated
     *      by waitForWriteConcern.
     */
    static void appendCommandWCStatus(BSONObjBuilder& result,
                                      const Status& awaitReplicationStatus,
                                      const WriteConcernResult& wcResult = WriteConcernResult());

    /**
     * If true, then testing commands are available. Defaults to false.
     *
     * Testing commands should conditionally register themselves by consulting this flag:
     *
     *     MONGO_INITIALIZER(RegisterMyTestCommand)(InitializerContext* context) {
     *         if (Command::testCommandsEnabled) {
     *             // Leaked intentionally: a Command registers itself when constructed.
     *             new MyTestCommand();
     *         }
     *         return Status::OK();
     *     }
     *
     * To make testing commands available by default, change the value to true before running any
     * mongo initializers:
     *
     *     int myMain(int argc, char** argv, char** envp) {
     *         Command::testCommandsEnabled = true;
     *         ...
     *         runGlobalInitializersOrDie(argc, argv, envp);
     *         ...
     *     }
     */
    ////mongod --setParameter=enableTestCommands
    static bool testCommandsEnabled;

    /**
     * Returns true if this a request for the 'help' information associated with the command.
     */
    static bool isHelpRequest(const BSONElement& helpElem);

    static const char kHelpFieldName[];

    /**
     * Generates a reply from the 'help' information associated with a command. The state of
     * the passed ReplyBuilder will be in kOutputDocs after calling this method.
     */
    static void generateHelpResponse(OperationContext* opCtx,
                                     rpc::ReplyBuilderInterface* replyBuilder,
                                     const Command& command);

    /**
     * This function checks if a command is a user management command by name.
     */
    static bool isUserManagementCommand(const std::string& name);

    /**
     * Checks to see if the client executing "opCtx" is authorized to run the given command with the
     * given parameters on the given named database.
     *
     * Returns Status::OK() if the command is authorized.  Most likely returns
     * ErrorCodes::Unauthorized otherwise, but any return other than Status::OK implies not
     * authorized.
     */
    static Status checkAuthorization(Command* c,
                                     OperationContext* opCtx,
                                     const OpMsgRequest& request);

    /**
     * Appends passthrough fields from a cmdObj to a given request.
     */
    static BSONObj appendPassthroughFields(const BSONObj& cmdObjWithPassthroughFields,
                                           const BSONObj& request);

    /**
     * Returns a copy of 'cmdObj' with a majority writeConcern appended.
     */
    static BSONObj appendMajorityWriteConcern(const BSONObj& cmdObj);

    /**
     * Returns true if the provided argument is one that is handled by the command processing layer
     * and should generally be ignored by individual command implementations. In particular,
     * commands that fail on unrecognized arguments must not fail for any of these.
     */
    static bool isGenericArgument(StringData arg) {
        // Not including "help" since we don't pass help requests through to the command parser.
        // If that changes, it should be added. When you add to this list, consider whether you
        // should also change the filterCommandRequestForPassthrough() function.
        return arg == "$audit" ||                        //
            arg == "$client" ||                          //
            arg == "$configServerState" ||               //
            arg == "$db" ||                              //
            arg == "allowImplicitCollectionCreation" ||  //
            arg == "$oplogQueryData" ||                  //
            arg == "$queryOptions" ||                    //
            arg == "$readPreference" ||                  //
            arg == "$replData" ||                        //
            arg == "$clusterTime" ||                     //
            arg == "maxTimeMS" ||                        //
            arg == "readConcern" ||                      //
            arg == "shardVersion" ||                     //
            arg == "tracking_info" ||                    //
            arg == "writeConcern" ||                     //
            arg == "lsid" ||                             //
            arg == "txnNumber" ||                        //
            false;  // These comments tell clang-format to keep this line-oriented.
    }

    /**
     * Rewrites cmdObj into a format safe to blindly forward to shards.
     *
     * This performs 2 transformations:
     * 1) $readPreference fields are moved into a subobject called $queryOptions. This matches the
     *    "wrapped" format historically used internally by mongos. Moving off of that format will be
     *    done as SERVER-29091.
     *
     * 2) Filter out generic arguments that shouldn't be blindly passed to the shards.  This is
     *    necessary because many mongos implementations of Command::run() just pass cmdObj through
     *    directly to the shards. However, some of the generic arguments fields are automatically
     *    appended in the egress layer. Removing them here ensures that they don't get duplicated.
     *
     * Ideally this function can be deleted once mongos run() implementations are more careful about
     * what they send to the shards.
     */
    static BSONObj filterCommandRequestForPassthrough(const BSONObj& cmdObj);

    /**
     * Rewrites reply into a format safe to blindly forward from shards to clients.
     *
     * Ideally this function can be deleted once mongos run() implementations are more careful about
     * what they return from the shards.
     */
    static void filterCommandReplyForPassthrough(const BSONObj& reply, BSONObjBuilder* output);
    static BSONObj filterCommandReplyForPassthrough(const BSONObj& reply);

private:
    //添加地方见Command::Command(  
    //所有的command都在_commands中保存
    static CommandMap* _commands;
    static CommandMap* _commandsByBestName;

    /**
     * Runs the command.
     *
     * The default implementation verifies that request has no document sections then forwards to
     * BasicCommand::run().
     *
     * For now commands should only implement if they need access to OP_MSG-specific functionality.
     */
    virtual bool enhancedRun(OperationContext* opCtx,
                             const OpMsgRequest& request,
                             BSONObjBuilder& result) = 0;

    // Counters for how many times this command has been executed and failed
    //db.serverStatus().metrics.commands命令查看
    Counter64 _commandsExecuted; //该命令执行次数 Command::Command
    Counter64 _commandsFailed;

    // The full name of the command
   //命令名，如"find" "insert" "update" "createIndexes" "deleteIndexes"
    const std::string _name;

    // Pointers to hold the metrics tree references
    ServerStatusMetricField<Counter64> _commandsExecutedMetric;
    ServerStatusMetricField<Counter64> _commandsFailedMetric;
};

/**
 * A subclass of Command that only cares about the BSONObj body and doesn't need access to document
 * sequences.
 */ //ErrmsgCommandDeprecated  CmdServerStatus等继承该类  AddShardCmd() : BasicCommand("addShard", "addshard") {}也继承该类
class BasicCommand : public Command {
public:
    using Command::Command;

    //
    // Interface for subclasses to implement
    //

    /**
     * run the given command
     * implement this...
     *
     * return value is true if succeeded.  if false, set errmsg text.
     */
    virtual bool run(OperationContext* opCtx,
                     const std::string& db,
                     const BSONObj& cmdObj,
                     BSONObjBuilder& result) = 0;

    /**
     * Checks if the client associated with the given OperationContext is authorized to run this
     * command. Default implementation defers to checkAuthForCommand.
     */
    virtual Status checkAuthForOperation(OperationContext* opCtx,
                                         const std::string& dbname,
                                         const BSONObj& cmdObj);

private:
    //
    // Deprecated virtual methods.
    //

    /**
     * Checks if the given client is authorized to run this command on database "dbname"
     * with the invocation described by "cmdObj".
     *
     * NOTE: Implement checkAuthForOperation that takes an OperationContext* instead.
     */ 
    virtual Status checkAuthForCommand(Client* client,
                                       const std::string& dbname,
                                       const BSONObj& cmdObj);

    /**
     * Appends to "*out" the privileges required to run this command on database "dbname" with
     * the invocation described by "cmdObj".  New commands shouldn't implement this, they should
     * implement checkAuthForOperation (which takes an OperationContext*), instead.
     */ //是否需要Privileges认证检查，例如CmdIsMaster命令就可以任意执行，而createUser则只能特定账号才可以
    //判断是否有执行dbname对应操作的权限
    virtual void addRequiredPrivileges(const std::string& dbname,
                                       const BSONObj& cmdObj,
                                       std::vector<Privilege>* out) {
        // The default implementation of addRequiredPrivileges should never be hit.
        fassertFailed(16940);
    }

    //
    // Methods provided for subclasses if they implement above interface.
    //

    /**
     * Calls run().
     */
    bool enhancedRun(OperationContext* opCtx,
                     const OpMsgRequest& request,
                     BSONObjBuilder& result) final;

    /**
     * Calls checkAuthForOperation.
     */
    Status checkAuthForRequest(OperationContext* opCtx, const OpMsgRequest& request) final;

    void uassertNoDocumentSequences(const OpMsgRequest& request);
};

/**
 * Deprecated. Do not add new subclasses.
 Commands.h (src\mongo\db):class ErrmsgCommandDeprecated : public BasicCommand {
 Commands_public.cpp (src\mongo\s\commands):class DropIndexesCmd : public ErrmsgCommandDeprecated {
 Commands_public.cpp (src\mongo\s\commands):    DropIndexesCmd() : ErrmsgCommandDeprecated("dropIndexes", "deleteIndexes") {}
 Commands_public.cpp (src\mongo\s\commands):class CreateIndexesCmd : public ErrmsgCommandDeprecated {
 Commands_public.cpp (src\mongo\s\commands):    CreateIndexesCmd() : ErrmsgCommandDeprecated("createIndexes") {}
 Commands_public.cpp (src\mongo\s\commands):class ReIndexCmd : public ErrmsgCommandDeprecated {
 Commands_public.cpp (src\mongo\s\commands):    ReIndexCmd() : ErrmsgCommandDeprecated("reIndex") {}
 Commands_public.cpp (src\mongo\s\commands):class CollectionModCmd : public ErrmsgCommandDeprecated {
 Commands_public.cpp (src\mongo\s\commands):    CollectionModCmd() : ErrmsgCommandDeprecated("collMod") {}
 Compact.cpp (src\mongo\db\commands):class CompactCmd : public ErrmsgCommandDeprecated {
 Compact.cpp (src\mongo\db\commands):    CompactCmd() : ErrmsgCommandDeprecated("compact") {}
 Copydb.cpp (src\mongo\db\commands):class CmdCopyDb : public ErrmsgCommandDeprecated {
 Copydb.cpp (src\mongo\db\commands):    CmdCopyDb() : ErrmsgCommandDeprecated("copydb") {}
 Copydb_start_commands.cpp (src\mongo\db\commands):class CmdCopyDbGetNonce : public ErrmsgCommandDeprecated {
 Copydb_start_commands.cpp (src\mongo\db\commands):    CmdCopyDbGetNonce() : ErrmsgCommandDeprecated("copydbgetnonce") {}
 Copydb_start_commands.cpp (src\mongo\db\commands):class CmdCopyDbSaslStart : public ErrmsgCommandDeprecated {
 Copydb_start_commands.cpp (src\mongo\db\commands):    CmdCopyDbSaslStart() : ErrmsgCommandDeprecated("copydbsaslstart") {}
 Cpuprofile.cpp (src\mongo\db\commands):class CpuProfilerCommand : public ErrmsgCommandDeprecated {
 Cpuprofile.cpp (src\mongo\db\commands):    CpuProfilerCommand(char const* name) : ErrmsgCommandDeprecated(name) {}
 Create_indexes.cpp (src\mongo\db\commands):class CmdCreateIndex : public ErrmsgCommandDeprecated {
 Create_indexes.cpp (src\mongo\db\commands):    CmdCreateIndex() : ErrmsgCommandDeprecated(kCommandName) {}
 Dbcommands.cpp (src\mongo\db\commands):class CmdRepairDatabase : public ErrmsgCommandDeprecated {
 Dbcommands.cpp (src\mongo\db\commands):    CmdRepairDatabase() : ErrmsgCommandDeprecated("repairDatabase") {}
 Dbcommands.cpp (src\mongo\db\commands):class CmdProfile : public ErrmsgCommandDeprecated {
 Dbcommands.cpp (src\mongo\db\commands):    CmdProfile() : ErrmsgCommandDeprecated("profile") {}
 Dbcommands.cpp (src\mongo\db\commands):class CmdDrop : public ErrmsgCommandDeprecated {
 Dbcommands.cpp (src\mongo\db\commands):    CmdDrop() : ErrmsgCommandDeprecated("drop") {}
 Dbcommands.cpp (src\mongo\db\commands):class CmdDatasize : public ErrmsgCommandDeprecated {
 Dbcommands.cpp (src\mongo\db\commands):    CmdDatasize() : ErrmsgCommandDeprecated("dataSize", "datasize") {}
 Dbcommands.cpp (src\mongo\db\commands):class CollectionStats : public ErrmsgCommandDeprecated {
 Dbcommands.cpp (src\mongo\db\commands):    CollectionStats() : ErrmsgCommandDeprecated("collStats", "collstats") {}
 Dbcommands.cpp (src\mongo\db\commands):class DBStats : public ErrmsgCommandDeprecated {
 Dbcommands.cpp (src\mongo\db\commands):    DBStats() : ErrmsgCommandDeprecated("dbStats", "dbstats") {}
 Dbhash.cpp (src\mongo\db\commands):class DBHashCmd : public ErrmsgCommandDeprecated {
 Dbhash.cpp (src\mongo\db\commands):    DBHashCmd() : ErrmsgCommandDeprecated("dbHash", "dbhash") {}
 driverHelpers.cpp (src\mongo\db\commands):class BasicDriverHelper : public ErrmsgCommandDeprecated {
 driverHelpers.cpp (src\mongo\db\commands):    BasicDriverHelper(const char* name) : ErrmsgCommandDeprecated(name) {}
 Drop_indexes.cpp (src\mongo\db\commands):class CmdReIndex : public ErrmsgCommandDeprecated {
 Drop_indexes.cpp (src\mongo\db\commands):    CmdReIndex() : ErrmsgCommandDeprecated("reIndex") {}
 Eval.cpp (src\mongo\db\commands):class CmdEval : public ErrmsgCommandDeprecated {
 Eval.cpp (src\mongo\db\commands):    CmdEval() : ErrmsgCommandDeprecated("eval", "$eval") {}
 Fail_point_cmd.cpp (src\mongo\db\commands):class FaultInjectCmd : public ErrmsgCommandDeprecated {
 Fail_point_cmd.cpp (src\mongo\db\commands):    FaultInjectCmd() : ErrmsgCommandDeprecated("configureFailPoint") {}
 Fsync.cpp (src\mongo\db\commands):class FSyncCommand : public ErrmsgCommandDeprecated {
 Fsync.cpp (src\mongo\db\commands):    FSyncCommand() : ErrmsgCommandDeprecated("fsync") {}
 Fsync.cpp (src\mongo\db\commands):class FSyncUnlockCommand : public ErrmsgCommandDeprecated {
 Fsync.cpp (src\mongo\db\commands):    FSyncUnlockCommand() : ErrmsgCommandDeprecated("fsyncUnlock") {}
 Generic.cpp (src\mongo\db\commands):class GetLogCmd : public ErrmsgCommandDeprecated {
 Generic.cpp (src\mongo\db\commands):    GetLogCmd() : ErrmsgCommandDeprecated("getLog") {}
 Geo_near_cmd.cpp (src\mongo\db\commands):class Geo2dFindNearCmd : public ErrmsgCommandDeprecated {
 Geo_near_cmd.cpp (src\mongo\db\commands):    Geo2dFindNearCmd() : ErrmsgCommandDeprecated("geoNear") {}
 Get_last_error.cpp (src\mongo\db\commands):class CmdGetLastError : public ErrmsgCommandDeprecated {
 Get_last_error.cpp (src\mongo\db\commands):    CmdGetLastError() : ErrmsgCommandDeprecated("getLastError", "getlasterror") {}
 Hashcmd.cpp (src\mongo\db\commands):class CmdHashElt : public ErrmsgCommandDeprecated {
 Hashcmd.cpp (src\mongo\db\commands):    CmdHashElt() : ErrmsgCommandDeprecated("_hashBSONElement"){};
 Haystack.cpp (src\mongo\db\commands):class GeoHaystackSearchCommand : public ErrmsgCommandDeprecated {
 Haystack.cpp (src\mongo\db\commands):    GeoHaystackSearchCommand() : ErrmsgCommandDeprecated("geoSearch") {}
 Merge_chunks_command.cpp (src\mongo\db\s):class MergeChunksCommand : public ErrmsgCommandDeprecated {
 Merge_chunks_command.cpp (src\mongo\db\s):    MergeChunksCommand() : ErrmsgCommandDeprecated("mergeChunks") {}
 Migration_destination_manager_legacy_commands.cpp (src\mongo\db\s):class RecvChunkStartCommand : public ErrmsgCommandDeprecated {
 Migration_destination_manager_legacy_commands.cpp (src\mongo\db\s):    RecvChunkStartCommand() : ErrmsgCommandDeprecated("_recvChunkStart") {}
 Mr.cpp (src\mongo\db\commands):class MapReduceCommand : public ErrmsgCommandDeprecated {
 Mr.cpp (src\mongo\db\commands):    MapReduceCommand() : ErrmsgCommandDeprecated("mapReduce", "mapreduce") {}
 Parameters.cpp (src\mongo\db\commands):class CmdGet : public ErrmsgCommandDeprecated {
 Parameters.cpp (src\mongo\db\commands):    CmdGet() : ErrmsgCommandDeprecated("getParameter") {}
 Parameters.cpp (src\mongo\db\commands):class CmdSet : public ErrmsgCommandDeprecated {
 Parameters.cpp (src\mongo\db\commands):    CmdSet() : ErrmsgCommandDeprecated("setParameter") {}
 Rename_collection_cmd.cpp (src\mongo\db\commands):class CmdRenameCollection : public ErrmsgCommandDeprecated {
 Rename_collection_cmd.cpp (src\mongo\db\commands):    CmdRenameCollection() : ErrmsgCommandDeprecated("renameCollection") {}
 Resync.cpp (src\mongo\db\repl):class CmdResync : public ErrmsgCommandDeprecated {
 Resync.cpp (src\mongo\db\repl):    CmdResync() : ErrmsgCommandDeprecated(kResyncFieldName) {}
 Set_shard_version_command.cpp (src\mongo\db\s):class SetShardVersion : public ErrmsgCommandDeprecated {
 Set_shard_version_command.cpp (src\mongo\db\s):    SetShardVersion() : ErrmsgCommandDeprecated("setShardVersion") {}
 Split_chunk_command.cpp (src\mongo\db\s):class SplitChunkCommand : public ErrmsgCommandDeprecated {
 Split_chunk_command.cpp (src\mongo\db\s):    SplitChunkCommand() : ErrmsgCommandDeprecated("splitChunk") {}
 Split_vector_command.cpp (src\mongo\db\s):class SplitVector : public ErrmsgCommandDeprecated {
 Split_vector_command.cpp (src\mongo\db\s):    SplitVector() : ErrmsgCommandDeprecated("splitVector") {}
 Test_commands.cpp (src\mongo\db\commands):class GodInsert : public ErrmsgCommandDeprecated {
 Test_commands.cpp (src\mongo\db\commands):    GodInsert() : ErrmsgCommandDeprecated("godinsert") {}
 Touch.cpp (src\mongo\db\commands):class TouchCmd : public ErrmsgCommandDeprecated {
 Touch.cpp (src\mongo\db\commands):    TouchCmd() : ErrmsgCommandDeprecated("touch") {}
 */
class ErrmsgCommandDeprecated : public BasicCommand {
    using BasicCommand::BasicCommand;
    //ErrmsgCommandDeprecated::run中会执行ErrmsgCommandDeprecated::errmsgRun
    bool run(OperationContext* opCtx,
             const std::string& db,
             const BSONObj& cmdObj,
             BSONObjBuilder& result) final;

    virtual bool errmsgRun(OperationContext* opCtx,
                           const std::string& db,
                           const BSONObj& cmdObj,
                           std::string& errmsg,
                           BSONObjBuilder& result) = 0;
};

}  // namespace mongo
