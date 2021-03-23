/*
 *    Copyright (C) 2013 10gen Inc.
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

#pragma once

#include <atomic>
#include <string>

#include "mongo/platform/atomic_proxy.h"
#include "mongo/platform/atomic_word.h"

/*
 * This file defines the storage for options that come from the command line related to data file
 * persistence.  Many executables that can access data files directly such as mongod and certain
 * tools use these variables, but each executable may have a different set of command line flags
 * that allow the user to change a different subset of these options.
 */

namespace mongo {

//注意StorageGlobalParams和ServerGlobalParams的区别:StorageGlobalParams中存储相关参数设置，ServerGlobalParams其他配置

//变量赋值在storeMongodOptions
struct StorageGlobalParams {
    // Default data directory for mongod when running in non-config server mode.
    static const char* kDefaultDbPath;

    // Default data directory for mongod when running as the config database of
    // a sharded cluster.
    static const char* kDefaultConfigDbPath; 

    // --storageEngine
    // storage engine for this instance of mongod.
    std::string engine = "wiredTiger";

    // True if --storageEngine was passed on the command line, and false otherwise.
    bool engineSetByUser = false; //如果配置文件中有storage.engine配置，则为true，见storeMongodOptions

    // The directory where the mongod instance stores its data.
    //数据目录
    std::string dbpath = kDefaultDbPath;

    // --upgrade
    // Upgrades the on-disk data format of the files specified by the --dbpath to the
    // latest version, if needed.
    bool upgrade = false;

    // --repair
    // Runs a repair routine on all databases. This is equivalent to shutting down and
    // running the repairDatabase database command on all databases.
    bool repair = false;

    // --repairpath
    // Specifies the root directory containing MongoDB data files to use for the --repair
    // operation.
    // Default: A _tmp directory within the path specified by the dbPath option.
    std::string repairpath;

    // The intention here is to enable the journal by default if we are running on a 64 bit system.
    bool dur = (sizeof(void*) == 8);  // --dur durability (now --journal)


    // --journalCommitInterval
    static const int kMaxJournalCommitIntervalMs;

    /*
    storage:
   dbPath: <string>
   journal:
      enabled: <boolean>
      commitIntervalMs: <num>
    */
    /* MongoDB 里的 journal 行为 主要由2个参数控制，storage.journal.enabled 决定是否开启journal，
    storage.journal.commitInternalMs 决定 journal 刷盘的间隔，默认为100ms，用户也可以通过写入时指定 
    writeConcern 为 {j: ture} 来每次写入时都确保 journal 刷盘。 */
    AtomicInt32 journalCommitIntervalMs;

    // --notablescan
    // no table scans allowed
    AtomicBool noTableScan{false};

    // --directoryperdb
    // Stores each database鈥檚 files in its own folder in the data directory.
    // When applied to an existing system, the directoryPerDB option alters
    // the storage pattern of the data directory.
    bool directoryperdb = false;

    // --syncdelay
    // Controls how much time can pass before MongoDB flushes data to the data files
    // via an fsync operation.
    // Do not set this value on production systems.
    // In almost every situation, you should use the default setting.
    static const double kMaxSyncdelaySecs;
    AtomicDouble syncdelay{60.0};  // seconds between fsyncs

    // --queryableBackupMode
    // Puts MongoD into "read-only" mode. MongoD will not write any data to the underlying
    // filesystem. Note that read operations may require writes. For example, a sort on a large
    // dataset may fail if it requires spilling to disk.
    //createLockFile中生效使用
    bool readOnly = false;

    // --groupCollections
    // Dictate to the storage engine that it should attempt to create new MongoDB collections from
    // an existing underlying MongoDB database level resource if possible. This can improve
    // workloads that rely heavily on creating many collections within a database.
    bool groupCollections = false;
};

extern StorageGlobalParams storageGlobalParams;

}  // namespace mongo
