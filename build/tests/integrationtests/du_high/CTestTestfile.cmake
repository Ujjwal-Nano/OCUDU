# CMake generated Testfile for 
# Source directory: /home/user/OCUDU/tests/integrationtests/du_high
# Build directory: /home/user/OCUDU/build/tests/integrationtests/du_high
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/home/user/OCUDU/build/tests/integrationtests/du_high/du_high_test[1]_include.cmake")
add_test(mac_test_mode_adapter_test "mac_test_mode_adapter_test")
set_tests_properties(mac_test_mode_adapter_test PROPERTIES  _BACKTRACE_TRIPLES "/home/user/OCUDU/tests/integrationtests/du_high/CMakeLists.txt;41;add_test;/home/user/OCUDU/tests/integrationtests/du_high/CMakeLists.txt;0;")
subdirs("ntn")
set_directory_properties(PROPERTIES LABELS "du_high;integrationtest")
