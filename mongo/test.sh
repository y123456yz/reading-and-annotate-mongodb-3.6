killall -9 mongod 
rm -rf /data1/mongodb/363/logs/mongod.log
rm -rf ./test-mongodb
 python2 buildscripts/scons.py mongod -j 4


