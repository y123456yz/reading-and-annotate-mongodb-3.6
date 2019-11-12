#!/bin/bash  
  
for((i=1;i<=10;i++));  
do   
  echo "use test;\r\ndb.foo.findOne()" | /usr/local/mongodb3610/bin/mongo --port 13331
done  

