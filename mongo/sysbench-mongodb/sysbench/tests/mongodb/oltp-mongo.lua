-- Copyright (C) 2006-2017 Alexey Kopytov <akopytov@gmail.com>

-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.

-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.

-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

-- -----------------------------------------------------------------------------
-- Common code for OLTP benchmarks.
-- -----------------------------------------------------------------------------
--
-- Support for MonogoDB 
-- Copyright (C) 2017 Alexey Stroganov@Percona <alexey.stroganov@percona.com>
--



function mongodb_init()

   mongorover = require("mongorover")
   mongodb_client = mongorover.MongoClient.new("mongodb://" .. sysbench.opt.mongodb_host .. 
                                               ":" .. sysbench.opt.mongodb_port.."/?serverSelectionTryOnce=false")
   mongodb_database = mongodb_client:getDatabase(sysbench.opt.mongodb_db)

   conn={}
   for t = 1, sysbench.opt.tables do
      conn[t] = mongodb_database:getCollection("sbtest" .. t)
   end
end


function init()
   assert(event ~= nil,
          "this script is meant to be included by other OLTP scripts and " ..
             "should not be called directly.")

end

if sysbench.cmdline.command == nil then
   error("Command is required. Supported commands: prepare, prewarm, run, " ..
            "cleanup, help")
end

-- Command line options
sysbench.cmdline.options = {
   mongodb_db =
      {"MongoDB: database name", "sbtest_test"},
   mongodb_host =
      {"MongoDB: hostname", "localhost"},
   mongodb_port =
      {"MongoDB: port", "27017"},
   table_size =
      {"Number of rows per table", 10000},
   read_only =
      {"Read only workload", false},
   range_size =
      {"Range size for range SELECT queries", 100},
   tables =
      {"Number of tables", 1},
   point_selects =
      {"Number of point SELECT queries per transaction", 10},
   simple_ranges =
      {"Number of simple range SELECT queries per transaction", 1},
   sum_ranges =
      {"Number of SELECT SUM() queries per transaction", 1},
   order_ranges =
      {"Number of SELECT ORDER BY queries per transaction", 1},
   distinct_ranges =
      {"Number of SELECT DISTINCT queries per transaction", 1},
   index_updates =
      {"Number of UPDATE index queries per transaction", 1},
   non_index_updates =
      {"Number of UPDATE non-index queries per transaction", 1},
   delete_inserts =
      {"Number of DELETE/INSERT combination per transaction", 1},
   range_selects =
      {"Enable/disable all range SELECT queries", true},
   auto_inc =
   {"Use AUTO_INCREMENT column as Primary Key (for MySQL), " ..
       "or its alternatives in other DBMS. When disabled, use " ..
       "client-generated IDs", true},
   skip_trx =
      {"Don't start explicit transactions and execute all queries " ..
          "in the AUTOCOMMIT mode", false},
   secondary =
      {"Use a secondary index in place of the PRIMARY KEY", false},
   create_secondary =
      {"Create a secondary index in addition to the PRIMARY KEY", true}
}


-- Template strings of random digits with 11-digit groups separated by dashes
   
-- 10 groups, 119 characters
local c_value_template = "###########-###########-###########-" ..
   "###########-###########-###########-" ..
   "###########-###########-###########-" ..
   "###########"

-- 5 groups, 59 characters
local pad_value_template = "###########-###########-###########-" ..
   "###########-###########"

function get_c_value()
   return sysbench.rand.string(c_value_template)
end

function get_pad_value()
   return sysbench.rand.string(pad_value_template)
end

function create_table(table_num)
   local id_index_def, id_def
   local engine_def = ""
   local extra_table_options = ""
   local query

   if sysbench.opt.secondary then
     id_index_def = "KEY xid"
   else
     id_index_def = "PRIMARY KEY"
   end

   print(string.format("Creating table 'sbtest%d'...", table_num))

   print(string.format("Inserting %d records into 'sbtest%d'", sysbench.opt.table_size, table_num))

   local c_val
   local pad_val
      
   for i = 1, sysbench.opt.table_size do
      
      c_val = get_c_value()
      pad_val = get_pad_value()
      k_val = sb_rand(1, sysbench.opt.table_size)
      
      row = { _id = i, k = k_val, c = c_val, pad = pad_val }
      --print ( "i: ",i,"k: ",k_val,"c: ",c_val,"pad: ",pad_val)
      result = conn[table_num]:insert_one(row)
      --print (result)
   end      

   if sysbench.opt.create_secondary then
      print(string.format("Creating a secondary index on 'sbtest%d'...",
                          table_num))

      mongodb_database:command("createIndexes", "sbtest"..table_num , { indexes = {{ key = { k = 1}, name = "k"}}})
   end
end

-- Prepare the dataset. This command supports parallel execution, i.e. will
-- benefit from executing with --threads > 1 as long as --tables > 1
function cmd_prepare()

   mongodb_init()
   for i = sysbench.tid % sysbench.opt.threads + 1, sysbench.opt.tables,  sysbench.opt.threads do
     create_table(i)
   end
end

function cmd_cleanup()

   mongodb_init()
   for i = 1, sysbench.opt.tables do
      print(string.format("Dropping table 'sbtest%d'...", i))
      conn[i]:drop()
   end
end

sysbench.cmdline.commands = {
   prepare = {cmd_prepare, sysbench.cmdline.PARALLEL_COMMAND},
   warmup = {cmd_warmup, sysbench.cmdline.PARALLEL_COMMAND},
   prewarm = {cmd_warmup, sysbench.cmdline.PARALLEL_COMMAND},
   cleanup = {cmd_cleanup}
}


local function get_table_num()
   return sysbench.rand.uniform(1, sysbench.opt.tables)
end

local function get_id()
   return sysbench.rand.default(1, sysbench.opt.table_size)
end

function begin()
end

function commit()
end


function fetch_results(result_set)
  local result 
  for result in result_set do
  end
end


function execute_point_selects()
   local tnum = get_table_num()
   local i

   for i = 1, sysbench.opt.point_selects do
      local result
      local id = get_id()
      result=conn[tnum]:find_one({_id = id}, {c = 1, _id = 0})
   end
end

function execute_simple_ranges()
   local tnum = get_table_num()

   for i = 1, sysbench.opt["simple_ranges"] do
      local results
      local id = get_id()
      local id_max = id+sysbench.opt.range_size - 1
      
      results=conn[tnum]:find({_id = { ["$gte"] = id, ["$lte"] = id_max }}, { c = 1, _id = 0 })
      fetch_results(results)      
   end
end

function execute_sum_ranges()

   local tnum = get_table_num()

   for i = 1, sysbench.opt["sum_ranges"] do
      local results
      local id = get_id()
      local id_max = id+sysbench.opt.range_size - 1
    
      
      local aggregationPipeline ={ { ["$match"] = { _id = { ["$gte"] = id, ["$lte"] = id_max }}}, 
                                   { ["$group"] = { _id = BSONNull.new(), total = { ["$sum"] = "$k" }}},
                                   { ["$project"] = { _id = 0, total = 1 }} }

      results=conn[tnum]:aggregate(aggregationPipeline)
      fetch_results(results)
   end
end


function execute_order_ranges()
   local tnum = get_table_num()

   for i = 1, sysbench.opt["order_ranges"] do
      local results
      local id = get_id()
      local id_max = id+sysbench.opt.range_size - 1
      
      local aggregationPipeline ={ { ["$match"] = { _id = { ["$gte"] = id, ["$lte"] = id_max }}}, 
                                   { ["$sort"] = { c = 1 }},
                                   { ["$project"] = { _id = 0, c = 1 }} }

      results=conn[tnum]:aggregate(aggregationPipeline)
      fetch_results(results)
   end
end

function execute_distinct_ranges()

   local tnum = get_table_num()

   for i = 1, sysbench.opt["distinct_ranges"] do
      local results
      local id = get_id()
      local id_max = id+sysbench.opt.range_size - 1
      
      local aggregationPipeline = { { ["$match"] = { _id = { ["$gte"] = id, ["$lte"] = id_max }}}, 
                                    { ["$group"] = { _id = "$c" } }, 
                                    { ["$sort"]  = { _id = -1 } } } 
                              
      results=conn[tnum]:aggregate(aggregationPipeline)
      fetch_results(results)
   end
end

function execute_index_updates()
   local tnum = get_table_num()

   for i = 1, sysbench.opt.index_updates do
      local id=get_id()
      
      local result = conn[tnum]:update_one({_id = id }, {["$inc"] = {k = 1}})
   end
end

function execute_non_index_updates()
   local tnum = get_table_num()

   for i = 1, sysbench.opt.non_index_updates do
      local result
      local id=get_id()
      local c_val=get_c_value()
            
      result = conn[tnum]:update_one({_id = id }, {["$set"] = {c = c_val }})

   end
end

function execute_delete_inserts()
   local tnum = get_table_num()

   for i = 1, sysbench.opt.delete_inserts do
      local result
      local id = get_id()
      local k = get_id()
      local c_val=get_c_value()
      local pad_val=get_pad_value()

      result = conn[tnum]:delete_one({_id = id})      

      while not pcall(function () mongodb_database:command("insert", "sbtest" .. tnum  ,
                                             { query = { _id= id }, 
                                               update = { ["$set"] = { k = k, c=c_val, pad=pad_val} }, 
                                               upsert="true" }) end ) do
      end
   end
end


function thread_init(thread_id)
   mongodb_init()
end

function event()

   if not sysbench.opt.skip_trx then
      begin()
   end

   execute_point_selects()
   execute_simple_ranges()
   execute_sum_ranges()
   execute_order_ranges()
   execute_distinct_ranges()

   if not sysbench.opt.read_only then 
      execute_index_updates()
      execute_non_index_updates()
      execute_delete_inserts()
   end

   if not sysbench.opt.skip_trx then
      commit()
   end
end
