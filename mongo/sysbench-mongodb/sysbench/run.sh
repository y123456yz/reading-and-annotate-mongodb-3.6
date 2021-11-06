#https://www.percona.com/blog/2016/05/13/benchmark-mongodb-sysbench/
./sysbench --mongo-write-concern=1 --mongo-url="mongodb://root:123456@127.0.0.1:29018" --mongo-database-name=sbtest --test=./tests/mongodb/oltp.lua --oltp_table_size=600000000 --oltp_tables_count=10 --num-threads=240 --rand-type=pareto --report-interval=2 --max-requests=0 --max-time=600 --test=./tests/mongodb/insert.lua --num-threads=1 run

./sysbench --mongo-write-concern=1 --mongo-url="mongodb://127.0.0.1:29018" --mongo-database-name=sbtest --test=./tests/mongodb/oltp.lua --oltp_table_size=600000000 --oltp_tables_count=10 --num-threads=240 --rand-type=pareto --report-interval=2 --max-requests=0 --max-time=600 --test=./tests/mongodb/insert.lua --num-threads=1 run

./sysbench --mongo-write-concern=1 --mongo-url="mongodb://127.0.0.1:28017" --mongo-database-name=sbtest --test=./tests/mongodb/oltp.lua --oltp_table_size=600000000 --oltp_tables_count=10 --num-threads=240 --rand-type=pareto --report-interval=2 --max-requests=0 --max-time=600 --test=./tests/mongodb/insert.lua --num-threads=150 run
