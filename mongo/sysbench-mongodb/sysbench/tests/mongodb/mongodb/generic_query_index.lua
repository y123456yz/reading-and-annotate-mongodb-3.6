-- Copyright (C) 2016 Percona
pathtest = string.match(test, "(.*/)") or ""

dofile(pathtest .. "common.lua")

function thread_init(thread_id)
         db_connect()
	 set_vars()
end

function event(thread_id)

--   for i=1, sb_rand(2, 10) do
--	mongodb_generic_query("sbtest" .. sb_rand(1, oltp_tables_count), "{ \"_id\" : \"" .. sb_rand(1, oltp_table_size) .. "\"}",  "{\"k\" : \"1\"}")
--mongodb_generic_query("sbtest" .. sb_rand(1, oltp_tables_count), "{ \"_id\" : \"" .. sb_rand(1, oltp_table_size) .. "\"}",  "{\"_id\" : \"1\" }")
 --  end
-- mongodb_generic_query("sbtest" .. sb_rand(1, oltp_tables_count), "{\"k\" : \"42146\", \"c\" :\"     d55729785539-82315423214-28583100476-99980538530-37274432951-99300949531-02974694667-24361151310-20881638047-96238221685\"}",  "{\"k\" : \"1\"}")
mongodb_generic_query("sbtest" .. sb_rand(1, oltp_tables_count), "{\"k\" : 118341245, \"yangtest1\" : \"     85565803193-83712332876-10966722361-92777329337-55029877077-26410738394-06346243950-98290815742-44831243404-23372642962\"}", "{\"k\" : \"1\"}")
 mongodb_fake_commit()
 
end
