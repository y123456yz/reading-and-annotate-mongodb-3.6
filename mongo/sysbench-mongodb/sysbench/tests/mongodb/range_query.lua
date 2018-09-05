-- Copyright (C) 2016 Percona
pathtest = string.match(test, "(.*/)") or ""

dofile(pathtest .. "common.lua")

function thread_init(thread_id)
         db_connect()
	 set_vars()
end

function event(thread_id)
   
      range_start = sb_rand(1, oltp_table_size)
      range_end = range_start + oltp_range_size 
      mongodb_simple_range("sbtest" .. sb_rand(1,oltp_tables_count), sb_rand(1,oltp_tables_count), range_start, range_end)

   mongodb_fake_commit()
   
end
