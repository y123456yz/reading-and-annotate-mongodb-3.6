/**
 * Copyright (C) 2016 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/db/stats/operation_latency_histogram.h"

#include <algorithm>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/namespace_string.h"
#include "mongo/platform/bits.h"

namespace mongo {

//把时延按照这些维度拆分为不同区间 _getBucket来判断时延统计应该落再那个区间
const std::array<uint64_t, OperationLatencyHistogram::kMaxBuckets>
    OperationLatencyHistogram::kLowerBounds = {0,
                                               2,
                                               4,
                                               8,
                                               16,
                                               32,
                                               64,
                                               128,
                                               256,
                                               512,
                                               1024,
                                               2048,
                                               3072,
                                               4096,
                                               6144,
                                               8192,
                                               12288,
                                               16384,
                                               24576,
                                               32768,
                                               49152,
                                               65536,
                                               98304,
                                               131072,
                                               196608,
                                               262144,
                                               393216,
                                               524288,
                                               786432,
                                               1048576,
                                               1572864,
                                               2097152,
                                               4194304,
                                               8388608,
                                               16777216,
                                               33554432,
                                               67108864,
                                               134217728,
                                               268435456,
                                               536870912,
                                               1073741824,
                                               2147483648,
                                               4294967296,
                                               8589934592,
                                               17179869184,
                                               34359738368,
                                               68719476736,
                                               137438953472,
                                               274877906944,
                                               549755813888,
                                               1099511627776};


/*
ocloud_oFEAkecX_shard_1:PRIMARY> db.collection.latencyStats( { histograms:true}).pretty()
{
        "ns" : "cloud_track.collection",
        "shard" : "ocloud_oFEAkecX_shard_1",
        "host" : "bjcp1134:20015",
        "localTime" : ISODate("2020-12-12T11:26:51.790Z"),
        "latencyStats" : {
                "reads" : {
                        "histogram" : [
                                {
                                        "micros" : NumberLong(16),
                                        "count" : NumberLong(6)
                                },
                                {
                                        "micros" : NumberLong(32),
                                        "count" : NumberLong(19)
                                },
                                {
                                        "micros" : NumberLong(64),
                                        "count" : NumberLong(1)
                                },
                                {
                                        "micros" : NumberLong(512),
                                        "count" : NumberLong(1)
                                },
                                {
                                        "micros" : NumberLong(3072),
                                        "count" : NumberLong(1)
                                }
                        ],
                        "latency" : NumberLong(5559),
                        "ops" : NumberLong(28)
                },
                "writes" : {
                        "histogram" : [ ],
                        "latency" : NumberLong(0),
                        "ops" : NumberLong(0)
                },
                "commands" : {
                        "histogram" : [ ],
                        "latency" : NumberLong(0),
                        "ops" : NumberLong(0)
                }
        }
}
*/
//OperationLatencyHistogram::append调用
void OperationLatencyHistogram::_append(const HistogramData& data,
                                        const char* key,
                                        bool includeHistograms,
                                        BSONObjBuilder* builder) const {

    BSONObjBuilder histogramBuilder(builder->subobjStart(key));
    if (includeHistograms) {
        BSONArrayBuilder arrayBuilder(histogramBuilder.subarrayStart("histogram"));
        for (int i = 0; i < kMaxBuckets; i++) {
            if (data.buckets[i] == 0)
                continue;
            BSONObjBuilder entryBuilder(arrayBuilder.subobjStart());
            entryBuilder.append("micros", static_cast<long long>(kLowerBounds[i]));
            entryBuilder.append("count", static_cast<long long>(data.buckets[i]));
            entryBuilder.doneFast();
        }
        arrayBuilder.doneFast();
    }
    histogramBuilder.append("latency", static_cast<long long>(data.sum));
    histogramBuilder.append("ops", static_cast<long long>(data.entryCount));
    histogramBuilder.doneFast();
}

//读写db.serverStatus().opLatencies汇总相关计数，所有表的统计 ---全局纬度
//db.collection.latencyStats( { histograms:true})  --- 表纬度
//db.collection.latencyStats( { histograms:false}) --- 表纬度

//Top::appendGlobalLatencyStats调用
void OperationLatencyHistogram::append(bool includeHistograms, BSONObjBuilder* builder) const {
    _append(_reads, "reads", includeHistograms, builder);
    _append(_writes, "writes", includeHistograms, builder);
    _append(_commands, "commands", includeHistograms, builder);
}

/*
histogram: [
  { micros: NumberLong(1), count: NumberLong(10) },
  { micros: NumberLong(2), count: NumberLong(1) },
  { micros: NumberLong(4096), count: NumberLong(1) },
  { micros: NumberLong(16384), count: NumberLong(1000) },
  { micros: NumberLong(49152), count: NumberLong(100) }
]
This indicates that there were:

10 operations taking 1 microsecond or less,    10个操作时延小于1ms
1 operation in the range (1, 2] microseconds,   1个操作时延范围【1-2】
1 operation in the range (3072, 4096] microseconds, 1个操作时延范围【3072-4096】
1000 operations in the range (12288, 16384], and  1000个操作时延范围【12288-16384】
100 operations in the range (32768, 49152].  100个操作时延范围【32768-49152】
*/

//记录不同时间段慢日志的详细统计
//确定latency时延对应在[0-2]、(2-4]、(4-8]、(8-16]、(16-32]、(32-64]、(64-128]...中的那个区间  
//上面的区间分别对应buckets桶0，桶1，桶2，桶3等待，也就是[0-2]对应桶0、(2-4]对应桶1、(4-8]对应桶2、(8-16]对应桶3，依次类推
// Computes the log base 2 of value, and checks for cases of split buckets.
int OperationLatencyHistogram::_getBucket(uint64_t value) {
    // Zero is a special case since log(0) is undefined.
    if (value == 0) {
        return 0;
    }

    int log2 = 63 - countLeadingZeros64(value);
    // Half splits occur in range [2^11, 2^21) giving 10 extra buckets.
    if (log2 < 11) {
        return log2;
    } else if (log2 < 21) {
        int extra = log2 - 11;
        // Split value boundary is at (2^n + 2^(n+1))/2 = 2^n + 2^(n-1).
        // Which corresponds to (1ULL << log2) | (1ULL << (log2 - 1))
        // Which is equivalent to the following:
        uint64_t splitBoundary = 3ULL << (log2 - 1);
        if (value >= splitBoundary) {
            extra++;
        }
        return log2 + extra;
    } else {
        // Add all of the extra 10 buckets.
        return std::min(log2 + 10, kMaxBuckets - 1);
    }
}

//OperationLatencyHistogram::increment中调用
//读 写 command总操作自增，时延对应增加latency
void OperationLatencyHistogram::_incrementData(uint64_t latency, int bucket, HistogramData* data) {
    //落在bucket桶指定时延范围的对应操作数自增
	data->buckets[bucket]++;
	//该操作总计数
    data->entryCount++;
	//该操作总时延计数
    data->sum += latency;
}

/*
db.serverStatus().opLatencies(全局纬度) 
db.collection.latencyStats( { histograms:true}).pretty()(表级纬度) 
*/
//Top._globalHistogramStats全局(包含所有表)的操作及时延统计-全局纬度
//CollectionData.opLatencyHistogram是表级别的读、写、command统计-表纬度

//不同请求归类参考getReadWriteType
//Top::_incrementHistogram   操作和时延计数操作
void OperationLatencyHistogram::increment(uint64_t latency, Command::ReadWriteType type) {
	//确定latency时延对应在[0-2]、(2-4]、(4-8]、(8-16]、(16-32]、(32-64]、(64-128]...中的那个区间??
	int bucket = _getBucket(latency);
    switch (type) {
		//读时延累加，操作计数自增
        case Command::ReadWriteType::kRead:
            _incrementData(latency, bucket, &_reads);
            break;
		//写时延累加，操作计数自增
        case Command::ReadWriteType::kWrite:
            _incrementData(latency, bucket, &_writes);
            break;
		//command时延累加，操作计数自增
        case Command::ReadWriteType::kCommand:
            _incrementData(latency, bucket, &_commands);
            break;
        default:
            MONGO_UNREACHABLE;
    }
}

}  // namespace mongo
