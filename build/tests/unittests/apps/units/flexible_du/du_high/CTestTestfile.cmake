# CMake generated Testfile for 
# Source directory: /home/user/OCUDU/tests/unittests/apps/units/flexible_du/du_high
# Build directory: /home/user/OCUDU/build/tests/unittests/apps/units/flexible_du/du_high
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(du_high_remote_commands_test "du_high_remote_commands_test")
set_tests_properties(du_high_remote_commands_test PROPERTIES  _BACKTRACE_TRIPLES "/home/user/OCUDU/tests/unittests/apps/units/flexible_du/du_high/CMakeLists.txt;17;add_test;/home/user/OCUDU/tests/unittests/apps/units/flexible_du/du_high/CMakeLists.txt;0;")
subdirs("metrics")
set_directory_properties(PROPERTIES LABELS "du_high;apps")
