/**
*    Copyright (C) 2015 MongoDB Inc.
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

// Define to target byte order (1234 vs 4321)
#define MONGO_CONFIG_BYTE_ORDER 1234

// Define if building a debug build
// #undef MONGO_CONFIG_DEBUG_BUILD

// Defined if execinfo.h and backtrace are available
#define MONGO_CONFIG_HAVE_EXECINFO_BACKTRACE 1

// Defined if OpenSSL has the FIPS_mode_set function
// #undef MONGO_CONFIG_HAVE_FIPS_MODE_SET

// Defined if unitstd.h is available
#define MONGO_CONFIG_HAVE_HEADER_UNISTD_H 1

// Defined if memset_s is available
// #undef MONGO_CONFIG_HAVE_MEMSET_S

// Defined if a POSIX monotonic clock is available
#define MONGO_CONFIG_HAVE_POSIX_MONOTONIC_CLOCK 1

// Defined if pthread.h and pthread_setname_np are available
#define MONGO_CONFIG_HAVE_PTHREAD_SETNAME_NP 1

// Defined if std::make_unique is available
#define MONGO_CONFIG_HAVE_STD_MAKE_UNIQUE 1

// Defined if strnlen is available
#define MONGO_CONFIG_HAVE_STRNLEN 1

// A number, if we have some extended alignment ability
#define MONGO_CONFIG_MAX_EXTENDED_ALIGNMENT 64

// Defined if building an optimized build
#define MONGO_CONFIG_OPTIMIZED_BUILD 1

// Defined if SSL support is enabled
// #undef MONGO_CONFIG_SSL

// Defined if OpenSSL has SEQUENCE_ANY
// #undef MONGO_CONFIG_HAVE_ASN1_ANY_DEFINITIONS

// Defined if OpenSSL has `SSL_CTX_set_ecdh_auto` and `SSL_set_ecdh_auto`
// #undef MONGO_CONFIG_HAS_SSL_SET_ECDH_AUTO

// Defined if WiredTiger storage engine is enabled
#define MONGO_CONFIG_WIREDTIGER_ENABLED 1
