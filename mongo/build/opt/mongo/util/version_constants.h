/*    Copyright 2016 10gen Inc.
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

#include <initializer_list>

#include "mongo/base/string_data.h"

// NOTE: You almost certainly don't want to include this file directly. Instead, access this
// information via the VersionInfoInterface singleton. To enforce this restriction we only allow
// inclusion of this file from files that have explicitly whitelisted it. Additionally, since this
// file is only ever included in .cpp files, it makes some sense to use an unnamed namespace here.

#ifndef MONGO_UTIL_VERSION_CONSTANTS_H_WHITELISTED
#error "Cannot include mongo/util/version_constants.h"
#endif

namespace mongo {
namespace version {
namespace {

constexpr auto kVersion = "3.6.1"_sd;
constexpr int kMajorVersion = 3;
constexpr int kMinorVersion = 6;
constexpr int kPatchVersion = 1;
constexpr int kExtraVersion = 0;
constexpr auto kVersionExtraStr = ""_sd;
constexpr auto kGitVersion = "nogitversion"_sd;
constexpr auto kAllocator = "tcmalloc"_sd;
constexpr auto kJsEngine = "mozjs"_sd;

// TODO: In C++14, make these constexpr too.
const std::initializer_list<StringData> kModulesList = {  };
const std::initializer_list<VersionInfoInterface::BuildInfoTuple> kBuildEnvironment =
    { std::make_tuple("distmod", "", true, true), std::make_tuple("distarch", "x86_64", true, true), std::make_tuple("cc", "gcc: gcc (GCC) 8.2.0", true, false), std::make_tuple("ccflags", "-fno-omit-frame-pointer -fno-strict-aliasing -ggdb -pthread -static-libstdc++ -Wall -Wsign-compare -Wno-unknown-pragmas -Winvalid-pch -O2 -Wno-unused-local-typedefs -Wno-unused-function -Wno-deprecated-declarations -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-missing-braces -fstack-protector-strong -fno-builtin-memcmp", true, false), std::make_tuple("cxx", "g++: g++ (GCC) 8.2.0", true, false), std::make_tuple("cxxflags", "-Woverloaded-virtual -Wno-maybe-uninitialized -std=c++14", true, false), std::make_tuple("linkflags", "-pthread -Wl,-z,now -rdynamic -fstack-protector-strong -fuse-ld=gold -Wl,--build-id -Wl,--hash-style=gnu -Wl,-z,noexecstack -Wl,--warn-execstack -Wl,-z,relro", true, false), std::make_tuple("target_arch", "x86_64", true, true), std::make_tuple("target_os", "linux", true, false) };

}  // namespace
}  // namespace version
}  // namespace mongo
