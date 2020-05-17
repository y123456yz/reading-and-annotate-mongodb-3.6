# CMake generated Testfile for 
# Source directory: /root/yyz/mongo-c-driver-1.11.0/src/libmongoc
# Build directory: /root/yyz/mongo-c-driver-1.11.0/cmake-build/src/libmongoc
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-libmongoc "/root/yyz/mongo-c-driver-1.11.0/cmake-build/src/libmongoc/test-libmongoc")
set_tests_properties(test-libmongoc PROPERTIES  WORKING_DIRECTORY "/root/yyz/mongo-c-driver-1.11.0/src/libmongoc/../.." _BACKTRACE_TRIPLES "/root/yyz/mongo-c-driver-1.11.0/src/libmongoc/CMakeLists.txt;783;add_test;/root/yyz/mongo-c-driver-1.11.0/src/libmongoc/CMakeLists.txt;0;")
subdirs("build")
subdirs("examples")
subdirs("src")
subdirs("tests")
