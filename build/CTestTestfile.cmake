# CMake generated Testfile for 
# Source directory: /home/ayman/thesis/AgarLE
# Build directory: /home/ayman/thesis/AgarLE/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[GameEngine]=] "agario/test-engine")
set_tests_properties([=[GameEngine]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ayman/thesis/AgarLE/CMakeLists.txt;31;add_test;/home/ayman/thesis/AgarLE/CMakeLists.txt;0;")
add_test([=[GameEngine-Renderable]=] "agario/test-engine-renderable")
set_tests_properties([=[GameEngine-Renderable]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ayman/thesis/AgarLE/CMakeLists.txt;34;add_test;/home/ayman/thesis/AgarLE/CMakeLists.txt;0;")
add_test([=[LearningEnvironment]=] "environment/test-envs")
set_tests_properties([=[LearningEnvironment]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ayman/thesis/AgarLE/CMakeLists.txt;37;add_test;/home/ayman/thesis/AgarLE/CMakeLists.txt;0;")
add_test([=[OpenAI-Gym]=] "python" "-m" "tests")
set_tests_properties([=[OpenAI-Gym]=] PROPERTIES  WORKING_DIRECTORY ".." _BACKTRACE_TRIPLES "/home/ayman/thesis/AgarLE/CMakeLists.txt;42;add_test;/home/ayman/thesis/AgarLE/CMakeLists.txt;0;")
subdirs("agario")
subdirs("environment")
subdirs("utils")
subdirs("bench")
