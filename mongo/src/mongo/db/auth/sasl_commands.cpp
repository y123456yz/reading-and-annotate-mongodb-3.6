/*
 *    Copyright (C) 2012 10gen, Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kAccessControl

#include "mongo/platform/basic.h"


#include "mongo/base/init.h"
#include "mongo/base/status.h"
#include "mongo/base/string_data.h"
#include "mongo/bson/mutable/algorithm.h"
#include "mongo/bson/mutable/document.h"
#include "mongo/bson/util/bson_extract.h"
#include "mongo/client/sasl_client_authenticate.h"
#include "mongo/db/audit.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/authz_manager_external_state_mock.h"
#include "mongo/db/auth/authz_session_external_state_mock.h"
#include "mongo/db/auth/mongo_authentication_session.h"
#include "mongo/db/auth/sasl_authentication_session.h"
#include "mongo/db/auth/sasl_options.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/commands/authentication_commands.h"
#include "mongo/db/server_options.h"
#include "mongo/util/base64.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/sequence_util.h"
#include "mongo/util/stringutils.h"

namespace mongo {
namespace {

using std::stringstream;

const bool autoAuthorizeDefault = true;
/*
认证过程日志:
2019-03-27T11:03:58.065+0800 I NETWORK  [listener] connection accepted from 1 127.0.0.1:48714 #2 (1 connection now open)
2019-03-27T11:03:58.065+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 0
2019-03-27T11:03:58.065+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { isMaster: 1, client: { application: { name: "MongoDB Shell" }, driver: { name: "MongoDB Internal Client", version: "3.6.6" }, os: { type: "Linux", name: "CentOS Linux release 7.2.1511 (Core) ", architecture: "x86_64", version: "Kernel 3.10.0-327.el7.x86_64" } }, $db: "admin" } isMaster
2019-03-27T11:03:58.066+0800 I NETWORK  [conn----yangtest2] received client metadata from 127.0.0.1:48714 conn: { application: { name: "MongoDB Shell" }, driver: { name: "MongoDB Internal Client", version: "3.6.6" }, os: { type: "Linux", name: "CentOS Linux release 7.2.1511 (Core) ", architecture: "x86_64", version: "Kernel 3.10.0-327.el7.x86_64" } }
2019-03-27T11:03:58.066+0800 D NETWORK  [conn----yangtest2] Starting server-side compression negotiation
2019-03-27T11:03:58.066+0800 D NETWORK  [conn----yangtest2] Compression negotiation not requested by client
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 0
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 1
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { whatsmyuri: 1, $db: "admin" } whatsmyuri
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 1
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 2
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { buildinfo: 1.0, $db: "admin" } buildinfo
2019-03-27T11:03:58.066+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 2
2019-03-27T11:03:58.067+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 3
2019-03-27T11:03:58.067+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { isMaster: 1.0, $clusterTime: { clusterTime: Timestamp(1553655835, 1), signature: { hash: BinData(0, B7A37AAB82E8E496CBDB5F525844062F4DDD8EDD), keyId: 6660292243598868500 } }, $db: "admin" } isMaster
2019-03-27T11:03:58.067+0800 D NETWORK  [conn----yangtest2] Starting server-side compression negotiation
2019-03-27T11:03:58.067+0800 D NETWORK  [conn----yangtest2] Compression negotiation not requested by client
2019-03-27T11:03:58.067+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 3
2019-03-27T11:03:58.068+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 4
2019-03-27T11:03:58.068+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { saslStart: 1, mechanism: "SCRAM-SHA-1", payload: "xxx", $db: "admin" } saslStart
2019-03-27T11:03:58.068+0800 D TRACKING [conn----yangtest2] Cmd: saslStart, TrackingId: 5c9ae81e7156c1bc05e53c9d
2019-03-27T11:03:58.068+0800 D EXECUTOR [conn----yangtest2] Scheduling remote command request: RemoteCommand 87 -- target:172.23.240.29:27018 db:admin expDate:2019-03-27T11:04:28.068+0800 cmd:{ usersInfo: [ { user: "root", db: "admin" } ], showPrivileges: true, showCredentials: true, showAuthenticationRestrictions: true, maxTimeMS: 30000 }  thread id:21360
2019-03-27T11:03:58.068+0800 D ASIO     [conn----yangtest2] startCommand: RemoteCommand 87 -- target:172.23.240.29:27018 db:admin expDate:2019-03-27T11:04:28.068+0800 cmd:{ usersInfo: [ { user: "root", db: "admin" } ], showPrivileges: true, showCredentials: true, showAuthenticationRestrictions: true, maxTimeMS: 30000 }
2019-03-27T11:03:58.068+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Initiating asynchronous command: RemoteCommand 87 -- target:172.23.240.29:27018 db:admin expDate:2019-03-27T11:04:28.068+0800 cmd:{ usersInfo: [ { user: "root", db: "admin" } ], showPrivileges: true, showCredentials: true, showAuthenticationRestrictions: true, maxTimeMS: 30000 }
2019-03-27T11:03:58.068+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Compressing message with snappy
2019-03-27T11:03:58.068+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Starting asynchronous command 87 on host 172.23.240.29:27018
2019-03-27T11:03:58.069+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Decompressing message with snappy
2019-03-27T11:03:58.069+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Request 87 finished with response: { users: [ { _id: "admin.root", user: "root", db: "admin", credentials: { SCRAM-SHA-1: { iterationCount: 10000, salt: "+N4XBhQR9uwjq/+8KO3NZg==", storedKey: "A65s2PyCMgmdtgXy9eOdMPMK/NM=", serverKey: "eUFi4aFow13ZYVo6Ex6TZdwriZI=" } }, roles: [ { role: "root", db: "admin" } ], inheritedRoles: [ { role: "root", db: "admin" } ], inheritedPrivileges: [ { resource: { cluster: true }, actions: [ "addShard", "appendOplogNote", "applicationMessage", "authSchemaUpgrade", "cleanupOrphaned", "connPoolStats", "connPoolSync", "cpuProfiler", "flushRouterConfig", "forceUUID", "fsync", "getCmdLineOpts", "getLog", "getParameter", "getShardMap", "hostInfo", "inprog", "invalidateUserCache", "killAnySession", "killop", "listCursors", "listDatabases", "listSessions", "listShards", "logRotate", "netstat", "removeShard", "replSetConfigure", "replSetGetConfig", "replSetGetStatus", "replSetResizeOplog", "replSetStateChange", "resync", "serverStatus", "setParameter", "shardingState", "shutdown", "top", "touch", "unlock", "useUUID" ] }, { resource: { db: "", collection: "" }, actions: [ "bypassDocumentValidation", "changeCustomData", "changePassword", "changeStream", "collMod", "collStats", "compact", "convertToCapped", "createCollection", "createIndex", "createRole", "createUser", "dbHash", "dbStats", "dropCollection", "dropDatabase", "dropIndex", "dropRole", "dropUser", "emptycapped", "enableProfiler", "enableSharding", "find", "getShardVersion", "grantRole", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheIndexFilter", "planCacheRead", "planCacheWrite", "reIndex", "remove", "renameCollectionSameDB", "repairDatabase", "revokeRole", "setAuthenticationRestriction", "splitChunk", "splitVector", "storageDetails", "update", "validate", "viewRole", "viewUser" ] }, { resource: { db: "config", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.replset" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "
killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.profile" }, actions: [ "changeStream", "collStats", "convertToCapped", "createCollection", "dbHash", "dbStats", "dropCollection", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "$setFeatureCompatibilityVersion", collection: "version" }, actions: [ "insert", "remove", "update" ] }, { resource: { db: "", collection: "system.users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "update" ] }, { resource: { db: "admin", collection: "system.users" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.roles" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.version" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.new_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.backup_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.js" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "dropIndex", "emptycapped", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "renameCollectionSameDB", "update" ] }, { resource: { db: "config", collection: "settings" }, actions: [ "find", "insert", "update" ] }, { resource: { anyResource: true }, actions: [ "collStats", "listCollections", "listIndexes", "validate" ] }, { resource: { db: "admin", collection: "tempusers" }, actions: [ "find" ] }, { resource: { db: "admin", collection: "temproles" }, actions: [ "find" ] } ], inheritedAuthenticationRestrictions: [], authenticationRestrictions: [] } ], ok: 1.0, operationTime: Timestamp(1553655836, 1), $replData: { term: 12, lastOpCommitted: { ts: Timestamp(1553655836, 1), t: 12 }, lastOpVisible: { ts: Timestamp(1553655836, 1), t: 12 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: -1 }, $gleStats: { lastOpTime: { ts: Timestamp(1553655835, 1), t: 12 }, electionId: ObjectId('7fffffff000000000000000c') }, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-27T11:03:58.069+0800 D EXECUTOR [NetworkInterfaceASIO-ShardRegistry-0] Received remote response: RemoteResponse --  cmd:{ users: [ { _id: "admin.root", user: "root", db: "admin", credentials: { SCRAM-SHA-1: { iterationCount: 10000, salt: "+N4XBhQR9uwjq/+8KO3NZg==", storedKey: "A65s2PyCMgmdtgXy9eOdMPMK/NM=", serverKey: "eUFi4aFow13ZYVo6Ex6TZdwriZI=" } }, roles: [ { role: "root", db: "admin" } ], inheritedRoles: [ { role: "root", db: "admin" } ], inheritedPrivileges: [ { resource: { cluster: true }, actions: [ "addShard", "appendOplogNote", "applicationMessage", "authSchemaUpgrade", "cleanupOrphaned", "connPoolStats", "connPoolSync", "cpuProfiler", "flushRouterConfig", "forceUUID", "fsync", "getCmdLineOpts", "getLog", "getParameter", "getShardMap", "hostInfo", "inprog", "invalidateUserCache", "killAnySession", "killop", "listCursors", "listDatabases", "listSessions", "listShards", "logRotate", "netstat", "removeShard", "replSetConfigure", "replSetGetConfig", "replSetGetStatus", "replSetResizeOplog", "replSetStateChange", "resync", "serverStatus", "setParameter", "shardingState", "shutdown", "top", "touch", "unlock", "useUUID" ] }, { resource: { db: "", collection: "" }, actions: [ "bypassDocumentValidation", "changeCustomData", "changePassword", "changeStream", "collMod", "collStats", "compact", "convertToCapped", "createCollection", "createIndex", "createRole", "createUser", "dbHash", "dbStats", "dropCollection", "dropDatabase", "dropIndex", "dropRole", "dropUser", "emptycapped", "enableProfiler", "enableSharding", "find", "getShardVersion", "grantRole", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheIndexFilter", "planCacheRead", "planCacheWrite", "reIndex", "remove", "renameCollectionSameDB", "repairDatabase", "revokeRole", "setAuthenticationRestriction", "splitChunk", "splitVector", "storageDetails", "update", "validate", "viewRole", "viewUser" ] }, { resource: { db: "config", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.replset" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find
", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.profile" }, actions: [ "changeStream", "collStats", "convertToCapped", "createCollection", "dbHash", "dbStats", "dropCollection", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "$setFeatureCompatibilityVersion", collection: "version" }, actions: [ "insert", "remove", "update" ] }, { resource: { db: "", collection: "system.users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "update" ] }, { resource: { db: "admin", collection: "system.users" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.roles" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.version" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.new_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.backup_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.js" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "dropIndex", "emptycapped", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "renameCollectionSameDB", "update" ] }, { resource: { db: "config", collection: "settings" }, actions: [ "find", "insert", "update" ] }, { resource: { anyResource: true }, actions: [ "collStats", "listCollections", "listIndexes", "validate" ] }, { resource: { db: "admin", collection: "tempusers" }, actions: [ "find" ] }, { resource: { db: "admin", collection: "temproles" }, actions: [ "find" ] } ], inheritedAuthenticationRestrictions: [], authenticationRestrictions: [] } ], ok: 1.0, operationTime: Timestamp(1553655836, 1), $replData: { term: 12, lastOpCommitted: { ts: Timestamp(1553655836, 1), t: 12 }, lastOpVisible: { ts: Timestamp(1553655836, 1), t: 12 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: -1 }, $gleStats: { lastOpTime: { ts: Timestamp(1553655835, 1), t: 12 }, electionId: ObjectId('7fffffff000000000000000c') }, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-27T11:03:58.070+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 4
2019-03-27T11:03:58.078+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 5
2019-03-27T11:03:58.078+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { saslContinue: 1, payload: BinData(0, 633D626977732C723D76514341592B6C69356A7871564A79564277656648576B5958315557583579442F4B6B6A2B585431337265692F644E37304A6C626B4B7439546C516656...), conversationId: 1, $db: "admin" } saslContinue
2019-03-27T11:03:58.078+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 5
2019-03-27T11:03:58.078+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 6
2019-03-27T11:03:58.078+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { saslContinue: 1, payload: BinData(0, ), conversationId: 1, $db: "admin" } saslContinue
2019-03-27T11:03:58.078+0800 D TRACKING [conn----yangtest2] Cmd: saslContinue, TrackingId: 5c9ae81e7156c1bc05e53ca0
2019-03-27T11:03:58.079+0800 D EXECUTOR [conn----yangtest2] Scheduling remote command request: RemoteCommand 89 -- target:172.23.240.29:27018 db:admin expDate:2019-03-27T11:04:28.079+0800 cmd:{ usersInfo: [ { user: "root", db: "admin" } ], showPrivileges: true, showCredentials: true, showAuthenticationRestrictions: true, maxTimeMS: 30000 }  thread id:21360
2019-03-27T11:03:58.079+0800 D ASIO     [conn----yangtest2] startCommand: RemoteCommand 89 -- target:172.23.240.29:27018 db:admin expDate:2019-03-27T11:04:28.079+0800 cmd:{ usersInfo: [ { user: "root", db: "admin" } ], showPrivileges: true, showCredentials: true, showAuthenticationRestrictions: true, maxTimeMS: 30000 }
2019-03-27T11:03:58.079+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Initiating asynchronous command: RemoteCommand 89 -- target:172.23.240.29:27018 db:admin expDate:2019-03-27T11:04:28.079+0800 cmd:{ usersInfo: [ { user: "root", db: "admin" } ], showPrivileges: true, showCredentials: true, showAuthenticationRestrictions: true, maxTimeMS: 30000 }
2019-03-27T11:03:58.079+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Compressing message with snappy
2019-03-27T11:03:58.079+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Starting asynchronous command 89 on host 172.23.240.29:27018
2019-03-27T11:03:58.079+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Decompressing message with snappy
2019-03-27T11:03:58.079+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Request 89 finished with response: { users: [ { _id: "admin.root", user: "root", db: "admin", credentials: { SCRAM-SHA-1: { iterationCount: 10000, salt: "+N4XBhQR9uwjq/+8KO3NZg==", storedKey: "A65s2PyCMgmdtgXy9eOdMPMK/NM=", serverKey: "eUFi4aFow13ZYVo6Ex6TZdwriZI=" } }, roles: [ { role: "root", db: "admin" } ], inheritedRoles: [ { role: "root", db: "admin" } ], inheritedPrivileges: [ { resource: { cluster: true }, actions: [ "addShard", "appendOplogNote", "applicationMessage", "authSchemaUpgrade", "cleanupOrphaned", "connPoolStats", "connPoolSync", "cpuProfiler", "flushRouterConfig", "forceUUID", "fsync", "getCmdLineOpts", "getLog", "getParameter", "getShardMap", "hostInfo", "inprog", "invalidateUserCache", "killAnySession", "killop", "listCursors", "listDatabases", "listSessions", "listShards", "logRotate", "netstat", "removeShard", "replSetConfigure", "replSetGetConfig", "replSetGetStatus", "replSetResizeOplog", "replSetStateChange", "resync", "serverStatus", "setParameter", "shardingState", "shutdown", "top", "touch", "unlock", "useUUID" ] }, { resource: { db: "", collection: "" }, actions: [ "bypassDocumentValidation", "changeCustomData", "changePassword", "changeStream", "collMod", "collStats", "compact", "convertToCapped", "createCollection", "createIndex", "createRole", "createUser", "dbHash", "dbStats", "dropCollection", "dropDatabase", "dropIndex", "dropRole", "dropUser", "emptycapped", "enableProfiler", "enableSharding", "find", "getShardVersion", "grantRole", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheIndexFilter", "planCacheRead", "planCacheWrite", "reIndex", "remove", "renameCollectionSameDB", "repairDatabase", "revokeRole", "setAuthenticationRestriction", "splitChunk", "splitVector", "storageDetails", "update", "validate", "viewRole", "viewUser" ] }, { resource: { db: "config", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.replset" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "
killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.profile" }, actions: [ "changeStream", "collStats", "convertToCapped", "createCollection", "dbHash", "dbStats", "dropCollection", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "$setFeatureCompatibilityVersion", collection: "version" }, actions: [ "insert", "remove", "update" ] }, { resource: { db: "", collection: "system.users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "update" ] }, { resource: { db: "admin", collection: "system.users" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.roles" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.version" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.new_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.backup_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.js" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "dropIndex", "emptycapped", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "renameCollectionSameDB", "update" ] }, { resource: { db: "config", collection: "settings" }, actions: [ "find", "insert", "update" ] }, { resource: { anyResource: true }, actions: [ "collStats", "listCollections", "listIndexes", "validate" ] }, { resource: { db: "admin", collection: "tempusers" }, actions: [ "find" ] }, { resource: { db: "admin", collection: "temproles" }, actions: [ "find" ] } ], inheritedAuthenticationRestrictions: [], authenticationRestrictions: [] } ], ok: 1.0, operationTime: Timestamp(1553655836, 1), $replData: { term: 12, lastOpCommitted: { ts: Timestamp(1553655836, 1), t: 12 }, lastOpVisible: { ts: Timestamp(1553655836, 1), t: 12 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: -1 }, $gleStats: { lastOpTime: { ts: Timestamp(1553655835, 1), t: 12 }, electionId: ObjectId('7fffffff000000000000000c') }, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-27T11:03:58.079+0800 D EXECUTOR [NetworkInterfaceASIO-ShardRegistry-0] Received remote response: RemoteResponse --  cmd:{ users: [ { _id: "admin.root", user: "root", db: "admin", credentials: { SCRAM-SHA-1: { iterationCount: 10000, salt: "+N4XBhQR9uwjq/+8KO3NZg==", storedKey: "A65s2PyCMgmdtgXy9eOdMPMK/NM=", serverKey: "eUFi4aFow13ZYVo6Ex6TZdwriZI=" } }, roles: [ { role: "root", db: "admin" } ], inheritedRoles: [ { role: "root", db: "admin" } ], inheritedPrivileges: [ { resource: { cluster: true }, actions: [ "addShard", "appendOplogNote", "applicationMessage", "authSchemaUpgrade", "cleanupOrphaned", "connPoolStats", "connPoolSync", "cpuProfiler", "flushRouterConfig", "forceUUID", "fsync", "getCmdLineOpts", "getLog", "getParameter", "getShardMap", "hostInfo", "inprog", "invalidateUserCache", "killAnySession", "killop", "listCursors", "listDatabases", "listSessions", "listShards", "logRotate", "netstat", "removeShard", "replSetConfigure", "replSetGetConfig", "replSetGetStatus", "replSetResizeOplog", "replSetStateChange", "resync", "serverStatus", "setParameter", "shardingState", "shutdown", "top", "touch", "unlock", "useUUID" ] }, { resource: { db: "", collection: "" }, actions: [ "bypassDocumentValidation", "changeCustomData", "changePassword", "changeStream", "collMod", "collStats", "compact", "convertToCapped", "createCollection", "createIndex", "createRole", "createUser", "dbHash", "dbStats", "dropCollection", "dropDatabase", "dropIndex", "dropRole", "dropUser", "emptycapped", "enableProfiler", "enableSharding", "find", "getShardVersion", "grantRole", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheIndexFilter", "planCacheRead", "planCacheWrite", "reIndex", "remove", "renameCollectionSameDB", "repairDatabase", "revokeRole", "setAuthenticationRestriction", "splitChunk", "splitVector", "storageDetails", "update", "validate", "viewRole", "viewUser" ] }, { resource: { db: "config", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "enableSharding", "find", "getShardVersion", "indexStats", "insert", "killCursors", "listCollections", "listIndexes", "moveChunk", "planCacheRead", "remove", "splitChunk", "splitVector", "update" ] }, { resource: { db: "local", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.js" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "config", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "local", collection: "system.replset" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find
", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.profile" }, actions: [ "changeStream", "collStats", "convertToCapped", "createCollection", "dbHash", "dbStats", "dropCollection", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "$setFeatureCompatibilityVersion", collection: "version" }, actions: [ "insert", "remove", "update" ] }, { resource: { db: "", collection: "system.users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "update" ] }, { resource: { db: "admin", collection: "system.users" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.roles" }, actions: [ "changeStream", "collStats", "createIndex", "dbHash", "dbStats", "dropIndex", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.version" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.new_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "admin", collection: "system.backup_users" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.indexes" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.namespaces" }, actions: [ "changeStream", "collStats", "dbHash", "dbStats", "find", "killCursors", "listCollections", "listIndexes", "planCacheRead" ] }, { resource: { db: "", collection: "system.js" }, actions: [ "bypassDocumentValidation", "changeStream", "collMod", "collStats", "convertToCapped", "createCollection", "createIndex", "dbHash", "dbStats", "dropCollection", "dropIndex", "emptycapped", "find", "insert", "killCursors", "listCollections", "listIndexes", "planCacheRead", "remove", "renameCollectionSameDB", "update" ] }, { resource: { db: "config", collection: "settings" }, actions: [ "find", "insert", "update" ] }, { resource: { anyResource: true }, actions: [ "collStats", "listCollections", "listIndexes", "validate" ] }, { resource: { db: "admin", collection: "tempusers" }, actions: [ "find" ] }, { resource: { db: "admin", collection: "temproles" }, actions: [ "find" ] } ], inheritedAuthenticationRestrictions: [], authenticationRestrictions: [] } ], ok: 1.0, operationTime: Timestamp(1553655836, 1), $replData: { term: 12, lastOpCommitted: { ts: Timestamp(1553655836, 1), t: 12 }, lastOpVisible: { ts: Timestamp(1553655836, 1), t: 12 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: -1 }, $gleStats: { lastOpTime: { ts: Timestamp(1553655835, 1), t: 12 }, electionId: ObjectId('7fffffff000000000000000c') }, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-27T11:03:58.080+0800 I ACCESS   [conn----yangtest2] Successfully authenticated as principal root on admin
2019-03-27T11:03:58.080+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 6
2019-03-27T11:03:58.081+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 7
2019-03-27T11:03:58.081+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { getLog: "startupWarnings", $clusterTime: { clusterTime: Timestamp(1553655835, 1), signature: { hash: BinData(0, B7A37AAB82E8E496CBDB5F525844062F4DDD8EDD), keyId: 6660292243598868500 } }, $db: "admin" } getLog
2019-03-27T11:03:58.081+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 7
2019-03-27T11:03:58.081+0800 D SHARDING [conn----yangtest2] Command begin db: test msg id: 8
2019-03-27T11:03:58.081+0800 D SHARDING [conn----yangtest2] yang test   run command test.$cmd { isMaster: 1.0, forShell: 1.0, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, D07A1335C015A7A1604ADCC08D5860FCDA9A22AC), keyId: 6660292243598868500 } }, $db: "test" } isMaster
2019-03-27T11:03:58.081+0800 D NETWORK  [conn----yangtest2] Starting server-side compression negotiation
2019-03-27T11:03:58.082+0800 D NETWORK  [conn----yangtest2] Compression negotiation not requested by client
2019-03-27T11:03:58.082+0800 D SHARDING [conn----yangtest2] Command end db: test msg id: 8
2019-03-27T11:03:58.082+0800 D SHARDING [conn----yangtest2] Command begin db: test msg id: 9
2019-03-27T11:03:58.082+0800 D SHARDING [conn----yangtest2] yang test   run command test.$cmd { buildInfo: 1.0, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, D07A1335C015A7A1604ADCC08D5860FCDA9A22AC), keyId: 6660292243598868500 } }, $db: "test" } buildInfo
2019-03-27T11:03:58.082+0800 D SHARDING [conn----yangtest2] Command end db: test msg id: 9
2019-03-27T11:03:58.083+0800 D SHARDING [conn----yangtest2] Command begin db: test msg id: 10
2019-03-27T11:03:58.083+0800 D SHARDING [conn----yangtest2] yang test   run command test.$cmd { isMaster: 1.0, forShell: 1.0, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, D07A1335C015A7A1604ADCC08D5860FCDA9A22AC), keyId: 6660292243598868500 } }, $db: "test" } isMaster
2019-03-27T11:03:58.083+0800 D NETWORK  [conn----yangtest2] Starting server-side compression negotiation
2019-03-27T11:03:58.083+0800 D NETWORK  [conn----yangtest2] Compression negotiation not requested by client
2019-03-27T11:03:58.083+0800 D SHARDING [conn----yangtest2] Command end db: test msg id: 10
2019-03-27T11:03:58.083+0800 D SHARDING [conn----yangtest2] Command begin db: admin msg id: 11
2019-03-27T11:03:58.083+0800 D SHARDING [conn----yangtest2] yang test   run command admin.$cmd { replSetGetStatus: 1.0, forShell: 1.0, $clusterTime: { clusterTime: Timestamp(1553655837, 1), signature: { hash: BinData(0, D07A1335C015A7A1604ADCC08D5860FCDA9A22AC), keyId: 6660292243598868500 } }, $db: "admin" } replSetGetStatus
2019-03-27T11:03:58.083+0800 D SHARDING [conn----yangtest2] Command end db: admin msg id: 11
*/
class CmdSaslStart : public BasicCommand {
public:
    CmdSaslStart();
    virtual ~CmdSaslStart();

    virtual void addRequiredPrivileges(const std::string&,
                                       const BSONObj&,
                                       std::vector<Privilege>*) {}

    void redactForLogging(mutablebson::Document* cmdObj) override;

    virtual bool run(OperationContext* opCtx,
                     const std::string& db,
                     const BSONObj& cmdObj,
                     BSONObjBuilder& result);

    virtual void help(stringstream& help) const;
    virtual bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }
    virtual bool slaveOk() const {
        return true;
    }
    bool requiresAuth() const override {
        return false;
    }
};

class CmdSaslContinue : public BasicCommand {
public:
    CmdSaslContinue();
    virtual ~CmdSaslContinue();

    virtual void addRequiredPrivileges(const std::string&,
                                       const BSONObj&,
                                       std::vector<Privilege>*) {}

    virtual bool run(OperationContext* opCtx,
                     const std::string& db,
                     const BSONObj& cmdObj,
                     BSONObjBuilder& result);

    virtual void help(stringstream& help) const;
    virtual bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }
    virtual bool slaveOk() const {
        return true;
    }
    bool requiresAuth() const override {
        return false;
    }
};

CmdSaslStart cmdSaslStart;
CmdSaslContinue cmdSaslContinue;
Status buildResponse(const SaslAuthenticationSession* session,
                     const std::string& responsePayload,
                     BSONType responsePayloadType,
                     BSONObjBuilder* result) {
    result->appendIntOrLL(saslCommandConversationIdFieldName, session->getConversationId());
    result->appendBool(saslCommandDoneFieldName, session->isDone());

    if (responsePayload.size() > size_t(std::numeric_limits<int>::max())) {
        return Status(ErrorCodes::InvalidLength, "Response payload too long");
    }
    if (responsePayloadType == BinData) {
        result->appendBinData(saslCommandPayloadFieldName,
                              int(responsePayload.size()),
                              BinDataGeneral,
                              responsePayload.data());
    } else if (responsePayloadType == String) {
        result->append(saslCommandPayloadFieldName, base64::encode(responsePayload));
    } else {
        fassertFailed(4003);
    }

    return Status::OK();
}

Status extractConversationId(const BSONObj& cmdObj, int64_t* conversationId) {
    BSONElement element;
    Status status = bsonExtractField(cmdObj, saslCommandConversationIdFieldName, &element);
    if (!status.isOK())
        return status;

    if (!element.isNumber()) {
        return Status(ErrorCodes::TypeMismatch,
                      str::stream() << "Wrong type for field; expected number for " << element);
    }
    *conversationId = element.numberLong();
    return Status::OK();
}

Status extractMechanism(const BSONObj& cmdObj, std::string* mechanism) {
    return bsonExtractStringField(cmdObj, saslCommandMechanismFieldName, mechanism);
}

Status doSaslStep(const Client* client,
                  SaslAuthenticationSession* session,
                  const BSONObj& cmdObj,
                  BSONObjBuilder* result) {
    std::string payload;
    BSONType type = EOO;
    Status status = saslExtractPayload(cmdObj, &payload, &type);
    if (!status.isOK())
        return status;

    std::string responsePayload;
    // Passing in a payload and extracting a responsePayload
    status = session->step(payload, &responsePayload);

    if (!status.isOK()) {
        log() << session->getMechanism() << " authentication failed for "
              << session->getPrincipalId() << " on " << session->getAuthenticationDatabase()
              << " from client " << client->getRemote().toString() << " ; " << redact(status);

        sleepmillis(saslGlobalParams.authFailedDelay.load());
        // All the client needs to know is that authentication has failed.
        return AuthorizationManager::authenticationFailedStatus;
    }

    status = buildResponse(session, responsePayload, type, result);
    if (!status.isOK())
        return status;

    if (session->isDone()) {
        UserName userName(session->getPrincipalId(), session->getAuthenticationDatabase());
        status =
            session->getAuthorizationSession()->addAndAuthorizeUser(session->getOpCtxt(), userName);
        if (!status.isOK()) {
            return status;
        }

		//说明认证成功了
        if (!serverGlobalParams.quiet.load()) {
            log() << "Successfully authenticated as principal " << session->getPrincipalId()
                  << " on " << session->getAuthenticationDatabase();
        }
    }
    return Status::OK();
}

Status doSaslStart(const Client* client,
                   SaslAuthenticationSession* session,
                   const std::string& db,
                   const BSONObj& cmdObj,
                   BSONObjBuilder* result) {
    bool autoAuthorize = false;
    Status status = bsonExtractBooleanFieldWithDefault(
        cmdObj, saslCommandAutoAuthorizeFieldName, autoAuthorizeDefault, &autoAuthorize);
    if (!status.isOK())
        return status;

    std::string mechanism;
    status = extractMechanism(cmdObj, &mechanism);
    if (!status.isOK())
        return status;

    if (!sequenceContains(saslGlobalParams.authenticationMechanisms, mechanism) &&
        mechanism != "SCRAM-SHA-1") {
        // Always allow SCRAM-SHA-1 to pass to the first sasl step since we need to
        // handle internal user authentication, SERVER-16534
        result->append(saslCommandMechanismListFieldName,
                       saslGlobalParams.authenticationMechanisms);
        return Status(ErrorCodes::BadValue,
                      mongoutils::str::stream() << "Unsupported mechanism " << mechanism);
    }

    status = session->start(
        db, mechanism, saslGlobalParams.serviceName, saslGlobalParams.hostName, 1, autoAuthorize);
    if (!status.isOK())
        return status;

    return doSaslStep(client, session, cmdObj, result);
}

Status doSaslContinue(const Client* client,
                      SaslAuthenticationSession* session,
                      const BSONObj& cmdObj,
                      BSONObjBuilder* result) {
    int64_t conversationId = 0;
    Status status = extractConversationId(cmdObj, &conversationId);
    if (!status.isOK())
        return status;
    if (conversationId != session->getConversationId())
        return Status(ErrorCodes::ProtocolError, "sasl: Mismatched conversation id");

    return doSaslStep(client, session, cmdObj, result);
}

CmdSaslStart::CmdSaslStart() : BasicCommand(saslStartCommandName) {}
CmdSaslStart::~CmdSaslStart() {}

void CmdSaslStart::help(std::stringstream& os) const {
    os << "First step in a SASL authentication conversation.";
}

void CmdSaslStart::redactForLogging(mutablebson::Document* cmdObj) {
    mutablebson::Element element = mutablebson::findFirstChildNamed(cmdObj->root(), "payload");
    if (element.ok()) {
        element.setValueString("xxx").transitional_ignore();
    }
}

bool CmdSaslStart::run(OperationContext* opCtx,
                       const std::string& db,
                       const BSONObj& cmdObj,
                       BSONObjBuilder& result) {
    Client* client = Client::getCurrent();
    AuthenticationSession::set(client, std::unique_ptr<AuthenticationSession>());

    std::string mechanism;
    if (!extractMechanism(cmdObj, &mechanism).isOK()) {
        return false;
    }

    SaslAuthenticationSession* session =
        SaslAuthenticationSession::create(AuthorizationSession::get(client), db, mechanism);

    std::unique_ptr<AuthenticationSession> sessionGuard(session);

    session->setOpCtxt(opCtx);

    Status status = doSaslStart(client, session, db, cmdObj, &result);
    appendCommandStatus(result, status);

    if (session->isDone()) {
        audit::logAuthentication(client,
                                 session->getMechanism(),
                                 UserName(session->getPrincipalId(), db),
                                 status.code());
    } else {
        AuthenticationSession::swap(client, sessionGuard);
    }
    return status.isOK();
}

CmdSaslContinue::CmdSaslContinue() : BasicCommand(saslContinueCommandName) {}
CmdSaslContinue::~CmdSaslContinue() {}

void CmdSaslContinue::help(std::stringstream& os) const {
    os << "Subsequent steps in a SASL authentication conversation.";
}

bool CmdSaslContinue::run(OperationContext* opCtx,
                          const std::string& db,
                          const BSONObj& cmdObj,
                          BSONObjBuilder& result) {
    Client* client = Client::getCurrent();
    std::unique_ptr<AuthenticationSession> sessionGuard;
    AuthenticationSession::swap(client, sessionGuard);

    if (!sessionGuard || sessionGuard->getType() != AuthenticationSession::SESSION_TYPE_SASL) {
        return appendCommandStatus(
            result, Status(ErrorCodes::ProtocolError, "No SASL session state found"));
    }

    SaslAuthenticationSession* session =
        static_cast<SaslAuthenticationSession*>(sessionGuard.get());

    // Authenticating the __system@local user to the admin database on mongos is required
    // by the auth passthrough test suite.
    if (session->getAuthenticationDatabase() != db && !Command::testCommandsEnabled) {
        return appendCommandStatus(
            result,
            Status(ErrorCodes::ProtocolError,
                   "Attempt to switch database target during SASL authentication."));
    }

    session->setOpCtxt(opCtx);

    Status status = doSaslContinue(client, session, cmdObj, &result);
    appendCommandStatus(result, status);

    if (session->isDone()) {
        audit::logAuthentication(client,
                                 session->getMechanism(),
                                 UserName(session->getPrincipalId(), db),
                                 status.code());
    } else {
        AuthenticationSession::swap(client, sessionGuard);
    }

    return status.isOK();
}

// The CyrusSaslCommands Enterprise initializer is dependent on PreSaslCommands
MONGO_INITIALIZER_WITH_PREREQUISITES(PreSaslCommands, ("NativeSaslServerCore"))
(InitializerContext*) {
    if (!sequenceContains(saslGlobalParams.authenticationMechanisms, "MONGODB-CR"))
        CmdAuthenticate::disableAuthMechanism("MONGODB-CR");

    if (!sequenceContains(saslGlobalParams.authenticationMechanisms, "MONGODB-X509"))
        CmdAuthenticate::disableAuthMechanism("MONGODB-X509");

    // For backwards compatibility, in 3.0 we are letting MONGODB-CR imply general
    // challenge-response auth and hence SCRAM-SHA-1 is enabled by either specifying
    // SCRAM-SHA-1 or MONGODB-CR in the authenticationMechanism server parameter.
    if (!sequenceContains(saslGlobalParams.authenticationMechanisms, "SCRAM-SHA-1") &&
        sequenceContains(saslGlobalParams.authenticationMechanisms, "MONGODB-CR"))
        saslGlobalParams.authenticationMechanisms.push_back("SCRAM-SHA-1");

    return Status::OK();
}

}  // namespace
}  // namespace mongo
