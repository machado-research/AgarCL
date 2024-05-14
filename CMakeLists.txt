cmake_minimum_required(VERSION 2.8...3.20)
cmake_policy(SET CMP0048 NEW)

project(AgarLE
        VERSION 0.1
        DESCRIPTION "Agar.io Learning Environment"
        HOMEPAGE_URL "https://github.com/jondeaton/AgarLE"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# To configure for "release", then run cmake with:
# cmake -DCMAKE_BUILD_TYPE=Release


add_subdirectory(agario)
add_subdirectory(environment)
add_subdirectory(utils)
add_subdirectory(bench)

enable_testing()

find_package(GTest)
if (GTEST_FOUND)

    add_test(NAME GameEngine
             COMMAND agario/test-engine)

    add_test(NAME GameEngine-Renderable
             COMMAND agario/test-engine-renderable)

    add_test(NAME LearningEnvironment
             COMMAND environment/test-envs)

endif()

add_test(NAME OpenAI-Gym
        COMMAND python -m tests
        WORKING_DIRECTORY ..)
