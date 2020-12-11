// AUTO-GENERATED FILE DO NOT EDIT
// See src/mongo/db/auth/generate_action_types.py
/*    Copyright 2014 MongoDB, Inc.
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
#include <map>
#include <string>

#include "mongo/base/status.h"

namespace mongo {

    struct ActionType {
    public:

        explicit ActionType(uint32_t identifier) : _identifier(identifier) {};
        ActionType() {};

        uint32_t getIdentifier() const {
            return _identifier;
        }

        bool operator==(const ActionType& rhs) const;

        std::string toString() const;

        // Takes the string representation of a single action type and returns the corresponding
        // ActionType enum.
        static Status parseActionFromString(const std::string& actionString, ActionType* result);

        // Takes an ActionType and returns the string representation
        static std::string actionToString(const ActionType& action);

        static const ActionType addShard;
        static const ActionType advanceClusterTime;
        static const ActionType anyAction;
        static const ActionType appendOplogNote;
        static const ActionType applicationMessage;
        static const ActionType auditLogRotate;
        static const ActionType authCheck;
        static const ActionType authenticate;
        static const ActionType authSchemaUpgrade;
        static const ActionType bypassDocumentValidation;
        static const ActionType changeCustomData;
        static const ActionType changePassword;
        static const ActionType changeOwnPassword;
        static const ActionType changeOwnCustomData;
        static const ActionType changeStream;
        static const ActionType cleanupOrphaned;
        static const ActionType closeAllDatabases;
        static const ActionType collMod;
        static const ActionType collStats;
        static const ActionType compact;
        static const ActionType connPoolStats;
        static const ActionType connPoolSync;
        static const ActionType convertToCapped;
        static const ActionType cpuProfiler;
        static const ActionType createCollection;
        static const ActionType createDatabase;
        static const ActionType createIndex;
        static const ActionType createRole;
        static const ActionType createUser;
        static const ActionType dbHash;
        static const ActionType dbStats;
        static const ActionType dropAllRolesFromDatabase;
        static const ActionType dropAllUsersFromDatabase;
        static const ActionType dropCollection;
        static const ActionType dropDatabase;
        static const ActionType dropIndex;
        static const ActionType dropRole;
        static const ActionType dropUser;
        static const ActionType emptycapped;
        static const ActionType enableProfiler;
        static const ActionType enableSharding;
        static const ActionType find;
        static const ActionType flushRouterConfig;
        static const ActionType forceUUID;
        static const ActionType fsync;
        static const ActionType getCmdLineOpts;
        static const ActionType getLog;
        static const ActionType getParameter;
        static const ActionType getShardMap;
        static const ActionType getShardVersion;
        static const ActionType grantRole;
        static const ActionType grantPrivilegesToRole;
        static const ActionType grantRolesToRole;
        static const ActionType grantRolesToUser;
        static const ActionType hostInfo;
        static const ActionType impersonate;
        static const ActionType indexStats;
        static const ActionType inprog;
        static const ActionType insert;
        static const ActionType internal;
        static const ActionType invalidateUserCache;
        static const ActionType killAnySession;
        static const ActionType killCursors;
        static const ActionType killop;
        static const ActionType listCollections;
        static const ActionType listCursors;
        static const ActionType listDatabases;
        static const ActionType listIndexes;
        static const ActionType listSessions;
        static const ActionType listShards;
        static const ActionType logRotate;
        static const ActionType moveChunk;
        static const ActionType netstat;
        static const ActionType planCacheIndexFilter;
        static const ActionType planCacheRead;
        static const ActionType planCacheWrite;
        static const ActionType reIndex;
        static const ActionType remove;
        static const ActionType removeShard;
        static const ActionType renameCollection;
        static const ActionType renameCollectionSameDB;
        static const ActionType repairDatabase;
        static const ActionType replSetConfigure;
        static const ActionType replSetGetConfig;
        static const ActionType replSetGetStatus;
        static const ActionType replSetHeartbeat;
        static const ActionType replSetReconfig;
        static const ActionType replSetResizeOplog;
        static const ActionType replSetStateChange;
        static const ActionType resync;
        static const ActionType revokeRole;
        static const ActionType revokePrivilegesFromRole;
        static const ActionType revokeRolesFromRole;
        static const ActionType revokeRolesFromUser;
        static const ActionType serverStatus;
        static const ActionType setAuthenticationRestriction;
        static const ActionType setParameter;
        static const ActionType shardCollection;
        static const ActionType shardingState;
        static const ActionType shutdown;
        static const ActionType splitChunk;
        static const ActionType splitVector;
        static const ActionType storageDetails;
        static const ActionType top;
        static const ActionType touch;
        static const ActionType unlock;
        static const ActionType useUUID;
        static const ActionType update;
        static const ActionType updateRole;
        static const ActionType updateUser;
        static const ActionType validate;
        static const ActionType viewRole;
        static const ActionType viewUser;

        enum ActionTypeIdentifier {
            addShardValue,
            advanceClusterTimeValue,
            anyActionValue,
            appendOplogNoteValue,
            applicationMessageValue,
            auditLogRotateValue,
            authCheckValue,
            authenticateValue,
            authSchemaUpgradeValue,
            bypassDocumentValidationValue,
            changeCustomDataValue,
            changePasswordValue,
            changeOwnPasswordValue,
            changeOwnCustomDataValue,
            changeStreamValue,
            cleanupOrphanedValue,
            closeAllDatabasesValue,
            collModValue,
            collStatsValue,
            compactValue,
            connPoolStatsValue,
            connPoolSyncValue,
            convertToCappedValue,
            cpuProfilerValue,
            createCollectionValue,
            createDatabaseValue,
            createIndexValue,
            createRoleValue,
            createUserValue,
            dbHashValue,
            dbStatsValue,
            dropAllRolesFromDatabaseValue,
            dropAllUsersFromDatabaseValue,
            dropCollectionValue,
            dropDatabaseValue,
            dropIndexValue,
            dropRoleValue,
            dropUserValue,
            emptycappedValue,
            enableProfilerValue,
            enableShardingValue,
            findValue,
            flushRouterConfigValue,
            forceUUIDValue,
            fsyncValue,
            getCmdLineOptsValue,
            getLogValue,
            getParameterValue,
            getShardMapValue,
            getShardVersionValue,
            grantRoleValue,
            grantPrivilegesToRoleValue,
            grantRolesToRoleValue,
            grantRolesToUserValue,
            hostInfoValue,
            impersonateValue,
            indexStatsValue,
            inprogValue,
            insertValue,
            internalValue,
            invalidateUserCacheValue,
            killAnySessionValue,
            killCursorsValue,
            killopValue,
            listCollectionsValue,
            listCursorsValue,
            listDatabasesValue,
            listIndexesValue,
            listSessionsValue,
            listShardsValue,
            logRotateValue,
            moveChunkValue,
            netstatValue,
            planCacheIndexFilterValue,
            planCacheReadValue,
            planCacheWriteValue,
            reIndexValue,
            removeValue,
            removeShardValue,
            renameCollectionValue,
            renameCollectionSameDBValue,
            repairDatabaseValue,
            replSetConfigureValue,
            replSetGetConfigValue,
            replSetGetStatusValue,
            replSetHeartbeatValue,
            replSetReconfigValue,
            replSetResizeOplogValue,
            replSetStateChangeValue,
            resyncValue,
            revokeRoleValue,
            revokePrivilegesFromRoleValue,
            revokeRolesFromRoleValue,
            revokeRolesFromUserValue,
            serverStatusValue,
            setAuthenticationRestrictionValue,
            setParameterValue,
            shardCollectionValue,
            shardingStateValue,
            shutdownValue,
            splitChunkValue,
            splitVectorValue,
            storageDetailsValue,
            topValue,
            touchValue,
            unlockValue,
            useUUIDValue,
            updateValue,
            updateRoleValue,
            updateUserValue,
            validateValue,
            viewRoleValue,
            viewUserValue,

            actionTypeEndValue, // Should always be last in this enum
        };

        static const int NUM_ACTION_TYPES = actionTypeEndValue;

    private:

        uint32_t _identifier; // unique identifier for this action.
    };

    // String stream operator for ActionType
    std::ostream& operator<<(std::ostream& os, const ActionType& at);

} // namespace mongo
