-- Copyright (C) 2016 Percona
pathtest = string.match(test, "(.*/)") or ""

dofile(pathtest .. "common.lua")

function thread_init(thread_id)
         db_connect()
	 set_vars()
--         for i=1, oltp_tables_count do
 --              print("Creating index on sbtest" .. i .. "(k)" )
  --             mongodb_create_index("sbtest" .. i, "k", "c")
   --      end
end

function event(thread_id)

--   local upper = tonumber(oltp_inserts)
--   if upper < 3 then
--      upper = 3
--   end
--   for i=1, sb_rand(2, upper) do
	local c_val = sb_rand_str([[
     ##########-##########-#####]])
	local pad_val = sb_rand_str([[
     ###########-###########-###########-###########-###########]])
       mongodb_insert("sbtest" .. sb_rand(1, oltp_tables_count), sb_rand(1, oltp_table_size), pad_val, pad_val, pad_val, c_val, c_val)
--   end
   mongodb_fake_commit()
 
end
