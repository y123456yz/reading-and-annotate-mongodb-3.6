// @file dur_journal.h

/**
*    Copyright (C) 2010 10gen Inc.
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

#include <boost/filesystem/path.hpp>

#include "mongo/db/storage/mmap_v1/aligned_builder.h"
#include "mongo/db/storage/mmap_v1/dur_journalformat.h"
#include "mongo/db/storage/mmap_v1/logfile.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/mutex.h"
#include "mongo/util/concurrency/mutex.h"

namespace mongo {

class ClockSource;

namespace dur {

/** the writeahead journal for durability */
/*
journal 是 MongoDB 存储引擎层的概念，目前 MongoDB主要支持 mmapv1、wiredtiger、mongorocks 等存储引擎，
都支持配置journal。

MongoDB 所有的数据写入、读取最终都是调存储引擎层的接口来存储、读取数据，journal 是存储引擎存储数据时的一种辅助机制。

以wiredtiger 为例，如果不配置 journal，写入 wiredtiger 的数据，并不会立即持久化存储；而是每分钟会做一次
全量的checkpoint（storage.syncPeriodSecs配置项，默认为1分钟），将所有的数据持久化。如果中间出现宕机，
那么数据只能恢复到最近的一次checkpoint，这样最多可能丢掉1分钟的数据。

所以建议「一定要开启journal」，开启 journal 后，每次写入会记录一条操作日志（通过journal可以重新构造出写入
的数据）。这样即使出现宕机，启动时 Wiredtiger 会先将数据恢复到最近的一次checkpoint的点，然后重放后续的 
journal 操作日志来恢复数据。

MongoDB 里的 journal 行为 主要由2个参数控制，storage.journal.enabled 决定是否开启journal，
storage.journal.commitInternalMs 决定 journal 刷盘的间隔，默认为100ms，用户也可以通过写入时指定 
writeConcern 为 {j: ture} 来每次写入时都确保 journal 刷盘。

oplog 与 journal 的关系:http://www.mongoing.com/archives/3988 一次写入，会对应数据、索引，oplog的修改，而这3个修改，会对应一条journal操作日志。
*/
class Journal {
public:
    std::string dir;  // set by journalMakeDir() during initialization

    Journal();

    /** call during startup by journalMakeDir() */
    void init(ClockSource* cs, int64_t serverStartMs);

    /** check if time to rotate files.  assure a file is open.
        done separately from the journal() call as we can do this part
        outside of lock.
        thread: durThread()
     */
    void rotate();

    /** append to the journal file
    */
    void journal(const JSectHeader& h, const AlignedBuilder& b);

    boost::filesystem::path getFilePathFor(int filenumber) const;

    void cleanup(bool log);  // closes and removes journal files

    unsigned long long curFileId() const {
        return _curFileId;
    }

    void assureLogFileOpen() {
        stdx::lock_guard<SimpleMutex> lk(_curLogFileMutex);
        if (_curLogFile == 0)
            _open();
    }

    /** open a journal file to journal operations to. */
    void open();

private:
    /** check if time to rotate files.  assure a file is open.
     *  internally called with every commit
     */
    void _rotate(unsigned long long lsnOfCurrentJournalEntry);

    void _open();
    void closeCurrentJournalFile();
    void removeUnneededJournalFiles();

    unsigned long long _written = 0;  // bytes written so far to the current journal (log) file
    unsigned _nextFileNumber = 0;

    SimpleMutex _curLogFileMutex;

    LogFile* _curLogFile;           // use _curLogFileMutex
    unsigned long long _curFileId;  // current file id see JHeader::fileId

    struct JFile {
        std::string filename;
        unsigned long long lastEventTimeMs;
    };

    // files which have been closed but not unlinked (rotated out) yet
    // ordered oldest to newest
    std::list<JFile> _oldJournalFiles;  // use _curLogFileMutex

    // lsn related
    friend void setLastSeqNumberWrittenToSharedView(uint64_t seqNumber);
    friend void notifyPreDataFileFlush();
    friend void notifyPostDataFileFlush();
    void updateLSNFile(unsigned long long lsnOfCurrentJournalEntry);
    // data <= this time is in the shared view
    AtomicUInt64 _lastSeqNumberWrittenToSharedView;
    // data <= this time was in the shared view when the last flush to start started
    AtomicUInt64 _preFlushTime;
    // data <= this time is fsynced in the datafiles (unless hard drive controller is caching)
    AtomicUInt64 _lastFlushTime;
    AtomicWord<bool> _writeToLSNNeeded;

    ClockSource* _clock;
    int64_t _serverStartMs;
};
}
}
