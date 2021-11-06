// top.h : DB usage monitor.

/*    Copyright 2009 10gen Inc.
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

#include <boost/date_time/posix_time/posix_time.hpp>

#include "mongo/db/commands.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/stats/operation_latency_histogram.h"
#include "mongo/util/concurrency/mutex.h"
#include "mongo/util/net/message.h"
#include "mongo/util/string_map.h"

namespace mongo {

class ServiceContext;

/**
 * tracks usage by collection
 mongotop实现原理:
use admin
db.runCommand( { top: 1 } )
 */
class Top {
public:
    static Top& get(ServiceContext* service);

    Top() = default;

    struct UsageData {
        UsageData() : time(0), count(0) {}
        UsageData(const UsageData& older, const UsageData& newer);
        long long time;
        long long count;

        //Top::_record调用
        void inc(long long micros) {
            count++;
            time += micros;
        }
    };

    //db.runCommand( { top: 1 } )中的统计信息，包括op和时延
    //Top._usage成员为该类型，注意OperationLatencyHistogram和UsageMap的区别
    //map表中每个表占用一个，参考Top::record
    struct CollectionData {
        /**
         * constructs a diff
         */
        CollectionData() {}
        CollectionData(const CollectionData& older, const CollectionData& newer);
        //总的，下面的[queries,commands]
        UsageData total;
        
        //锁纬度
        UsageData readLock;
        UsageData writeLock;

        //表级别不同操作的时延统计，粒度相比OperationLatencyHistogram更小
        //请求类型纬度
        UsageData queries;
        UsageData getmore;
        UsageData insert;
        UsageData update;
        UsageData remove;
        UsageData commands;
        
        //读写db.serverStatus().opLatencies汇总相关计数，所有表的统计 ---全局纬度
        //db.collection.latencyStats( { histograms:true})  --- 表纬度
        //db.collection.latencyStats( { histograms:false}) --- 表纬度


        //表级别的读和写时延统计 
        //Top._globalHistogramStats全局(包含所有表)的操作及时延统计-全局纬度
        //CollectionData.opLatencyHistogram是表级别的读、写、command统计-表纬度

        //Top::_record中对该表的读、写、command进行统计
        //汇总型纬度
        OperationLatencyHistogram opLatencyHistogram;
    };

    enum class LockType {
        ReadLocked,
        WriteLocked,
        NotLocked,
    };
    //Top._usage  各种命令的详细统计记录在该map表中
    //map表中每个表占用一个，参考Top::record
    typedef StringMap<CollectionData> UsageMap;

public:
    void record(OperationContext* opCtx,
                StringData ns,
                LogicalOp logicalOp,
                LockType lockType,
                long long micros,
                bool command,
                Command::ReadWriteType readWriteType);

    void append(BSONObjBuilder& b);

    void cloneMap(UsageMap& out) const;

    void collectionDropped(StringData ns, bool databaseDropped = false);

    /**
     * Appends the collection-level latency statistics
     */
    void appendLatencyStats(StringData ns, bool includeHistograms, BSONObjBuilder* builder);

    /**
     * Increments the global histogram.
     */
    void incrementGlobalLatencyStats(OperationContext* opCtx,
                                     uint64_t latency,
                                     Command::ReadWriteType readWriteType);

    /**
     * Appends the global latency statistics.
     */
    void appendGlobalLatencyStats(bool includeHistograms, BSONObjBuilder* builder);

private:
    void _appendToUsageMap(BSONObjBuilder& b, const UsageMap& map) const;

    void _appendStatsEntry(BSONObjBuilder& b, const char* statsName, const UsageData& map) const;

    void _record(OperationContext* opCtx,
                 CollectionData& c,
                 LogicalOp logicalOp,
                 LockType lockType,
                 long long micros,
                 Command::ReadWriteType readWriteType);

    void _incrementHistogram(OperationContext* opCtx,
                             long long latency,
                             OperationLatencyHistogram* histogram,
                             Command::ReadWriteType readWriteType);

    mutable SimpleMutex _lock;
    //读写db.serverStatus().opLatencies汇总相关计数，所有表的统计 ---全局纬度
    //db.collection.latencyStats( { histograms:true})  --- 表纬度
    //db.collection.latencyStats( { histograms:false}) --- 表纬度


    
    //Top._globalHistogramStats全局(包含所有表)的操作及时延统计-全局纬度
    //CollectionData.opLatencyHistogram是表级别的读、写、command统计-表纬度
    OperationLatencyHistogram _globalHistogramStats;
    //每个命令详细的qps、时延统计   db.runCommand( { top: 1 } )获取
    UsageMap _usage;  //map表中每个表占用一个，参考Top::record
    std::string _lastDropped;
};

}  // namespace mongo
