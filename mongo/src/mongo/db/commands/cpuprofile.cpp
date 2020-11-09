// @file cpuprofile.cpp

/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.

 -bash-4.2# cd /path/on/
 -bash-4.2# 
 -bash-4.2# pprof /root/mongod	./mongod-host.prof 
 Using local file /root/mongod.
 Using local file ./mongod-host.prof.
 Welcome to pprof!	For help, type 'help'.
 (pprof) help
 Interactive pprof mode
 
 Commands:
   gv
   gv [focus] [-ignore1] [-ignore2]
	   Show graphical hierarchical display of current profile.	Without
	   any arguments, shows all samples in the profile.  With the optional
	   "focus" argument, restricts the samples shown to just those where
	   the "focus" regular expression matches a routine name on the stack
	   trace.
 
   web
   web [focus] [-ignore1] [-ignore2]
	   Like GV, but displays profile in your web browser instead of using
	   Ghostview. Works best if your web browser is already running.
	   To change the browser that gets used:
	   On Linux, set the /etc/alternatives/gnome-www-browser symlink.
	   On OS X, change the Finder association for SVG files.
 
   list [routine_regexp] [-ignore1] [-ignore2]
	   Show source listing of routines whose names match "routine_regexp"
 
   weblist [routine_regexp] [-ignore1] [-ignore2]
	  Displays a source listing of routines whose names match "routine_regexp"
	  in a web browser.  You can click on source lines to view the
	  corresponding disassembly.
 
   top [--cum] [-ignore1] [-ignore2]
   top20 [--cum] [-ignore1] [-ignore2]
   top37 [--cum] [-ignore1] [-ignore2]
	   Show top lines ordered by flat profile count, or cumulative count
	   if --cum is specified.  If a number is present after 'top', the
	   top K routines will be shown (defaults to showing the top 10)
 
   disasm [routine_regexp] [-ignore1] [-ignore2]
	   Show disassembly of routines whose names match "routine_regexp",
	   annotated with sample counts.
 
   callgrind
   callgrind [filename]
	   Generates callgrind file. If no filename is given, kcachegrind is called.
 
   help - This listing
   quit or ^D - End pprof
 
 For commands that accept optional -ignore tags, samples where any routine in
 the stack trace matches the regular expression in any of the -ignore
 parameters will be ignored.
 
 Further pprof details are available at this location (or one similar):
 
  /usr/doc/gperftools-2.0/cpu_profiler.html
  /usr/doc/gperftools-2.0/heap_profiler.html
 
 (pprof) top
 Total: 18 samples
		7  38.9%  38.9% 	   7  38.9% __pthread_cond_timedwait
		5  27.8%  66.7% 	   5  27.8% __read_nocancel
		1	5.6%  72.2% 	   1   5.6% __GI_____strtoull_l_internal
		1	5.6%  77.8% 	   1   5.6% __munmap
		1	5.6%  83.3% 	   1   5.6% __open_nocancel
		1	5.6%  88.9% 	   1   5.6% __strlen_sse2_pminub
		1	5.6%  94.4% 	   1   5.6% compressBound
		1	5.6% 100.0% 	   1   5.6% mongo::procparser::parseProcNetstat
		0	0.0% 100.0% 	   1   5.6% _IO_new_fclose
		0	0.0% 100.0% 	   1   5.6% _IO_new_file_close_it
 (pprof) quit

 */

/**
 * This module provides commands for starting and stopping the Google perftools
 * cpu profiler linked into mongod.
 *
 * The following command enables the not-currently-enabled profiler, and writes
 * the profile data to the specified "profileFilename."
 *     { _cpuProfilerStart: { profileFilename: '/path/on/mongod-host.prof' } }

 > db.adminCommand({ _cpuProfilerStart: { profileFilename: '/path/on/mongod-host.prof' } })
{ "ok" : 1 }
> 
 https://jira.mongodb.org/browse/SERVER-13476?jql=text%20~%20%22prof%22

 *
 * The following command disables the already-enabled profiler:
 *     db.adminCommand({ _cpuProfilerStop: 1})
 *
 * The commands defined here, and profiling, are only available when enabled at
 * build-time with the "--use-cpu-profiler" argument to scons.
 *
 * Example SCons command line:
 *
 *     scons --release --use-cpu-profiler
 */


#include "gperftools/profiler.h"

#include <string>
#include <vector>

#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/jsobj.h"

namespace mongo {

namespace {

/**
 * Common code for the implementation of cpu profiler commands.
 */
class CpuProfilerCommand : public ErrmsgCommandDeprecated {
public:
    CpuProfilerCommand(char const* name) : ErrmsgCommandDeprecated(name) {}
    virtual bool slaveOk() const {
        return true;
    }
    virtual bool adminOnly() const {
        return true;
    }
    virtual bool localHostOnlyIfNoAuth() {
        return true;
    }
    virtual void addRequiredPrivileges(const std::string& dbname,
                                       const BSONObj& cmdObj,
                                       std::vector<Privilege>* out) {
        ActionSet actions;
        actions.addAction(ActionType::cpuProfiler);
        out->push_back(Privilege(ResourcePattern::forClusterResource(), actions));
    }

    // This is an abuse of the global dbmutex.  We only really need to
    // ensure that only one cpuprofiler command runs at once; it would
    // be fine for it to run concurrently with other operations.
    virtual bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }
};

/**
 * Class providing implementation of the _cpuProfilerStart command.
 */
class CpuProfilerStartCommand : public CpuProfilerCommand {
public:
    CpuProfilerStartCommand() : CpuProfilerCommand(commandName) {}

    virtual bool errmsgRun(OperationContext* opCtx,
                           std::string const& db,
                           const BSONObj& cmdObj,
                           std::string& errmsg,
                           BSONObjBuilder& result);

    static char const* const commandName;
} cpuProfilerStartCommandInstance;

/**
 * Class providing implementation of the _cpuProfilerStop command.
 */
class CpuProfilerStopCommand : public CpuProfilerCommand {
public:
    CpuProfilerStopCommand() : CpuProfilerCommand(commandName) {}

    virtual bool errmsgRun(OperationContext* opCtx,
                           std::string const& db,
                           const BSONObj& cmdObj,
                           std::string& errmsg,
                           BSONObjBuilder& result);

    static char const* const commandName;
} cpuProfilerStopCommandInstance;

char const* const CpuProfilerStartCommand::commandName = "_cpuProfilerStart";
char const* const CpuProfilerStopCommand::commandName = "_cpuProfilerStop";

bool CpuProfilerStartCommand::errmsgRun(OperationContext* opCtx,
                                        std::string const& db,
                                        const BSONObj& cmdObj,
                                        std::string& errmsg,
                                        BSONObjBuilder& result) {
    // The DB lock here is just so we have IX on the global lock in order to prevent shutdown
    Lock::DBLock dbXLock(opCtx, db, MODE_X);
    OldClientContext ctx(opCtx, db, false /* no shard version checking */);

    std::string profileFilename = cmdObj[commandName]["profileFilename"].String();
    if (!::ProfilerStart(profileFilename.c_str())) {
        errmsg = "Failed to start profiler";
        return false;
    }
    return true;
}

bool CpuProfilerStopCommand::errmsgRun(OperationContext* opCtx,
                                       std::string const& db,
                                       const BSONObj& cmdObj,
                                       std::string& errmsg,
                                       BSONObjBuilder& result) {
    // The DB lock here is just so we have IX on the global lock in order to prevent shutdown
    Lock::DBLock dbXLock(opCtx, db, MODE_X);
    OldClientContext ctx(opCtx, db, false /* no shard version checking */);

    ::ProfilerStop();
    return true;
}

}  // namespace

}  // namespace mongo
