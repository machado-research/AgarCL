cmake_minimum_required(VERSION 2.8...3.20)
cmake_policy(SET CMP0048 NEW)

project(Agario
        VERSION 0.1
        DESCRIPTION "Agar.io Game Engine"
        HOMEPAGE_URL "https://github.com/AgarCL/AgarCL"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
# For C as well

set(DEBUG_FLAGS "-g -O0 -fpermissive -fsized-deallocation")
set(RELEASE_FLAGS "-O3 -fpermissive")
set(BENCH_FLAGS "-Ofast -fno-omit-frame-pointer")

# To configure for "release", then run cmake with:
# cmake -DCMAKE_BUILD_TYPE=Release

function(make_includable input_file output_file)
    file(READ ${input_file} content)
    set(delim "for_c++_include")
    set(content "R\"${delim}(\n${content})${delim}\"")
    file(WRITE ${output_file} "${content}")
endfunction(make_includable)

make_includable(rendering/shaders/vertex.glsl rendering/shaders/_vertex.glsl)
make_includable(rendering/shaders/fragment.glsl rendering/shaders/_fragment.glsl)

# Allow for USE_EGL
option(USE_EGL "Make environments headlessly renderable" ON)

# On Apple platforms, always disable EGL
if(APPLE)
  message(STATUS "macOS detected → disabling headless EGL support")
  set(USE_EGL OFF CACHE BOOL "Make environments headlessly renderable" FORCE)
endif()

if (USE_EGL)
  message(STATUS "Headlessly renderable (EGL ON)")
  add_definitions(-DUSE_EGL)
else()
  message(STATUS "Headlessly renderable (EGL OFF)")
endif()

message(STATUS "Final setting: USE_EGL=${USE_EGL}")

IF(APPLE)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    link_directories(/usr/local/lib)
ENDIF()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Mode: Release")
    set(CMAKE_CXX_FLAGS  "${RELEASE_FLAGS}")
else()
    message(STATUS "Mode: Debug")
    set(CMAKE_CXX_FLAGS "${DEBUG_FLAGS}")
endif()

include_directories("." "..")

set(AGARIO_CORE_SRC
        core/core.hpp           core/utils.hpp
        core/types.hpp          core/num_wrapper.hpp
        core/color.hpp
        core/Entities.hpp       core/Ball.hpp
        core/Player.hpp)

set(AGARIO_BOT_SRC
        bots/bots.hpp
        bots/Bot.hpp
        bots/ExampleBot.hpp
        bots/HungryBot.hpp
        bots/HungryShyBot.hpp
        bots/AggressiveBot.hpp)

set(AGARIO_ENGINE_SRC
        ${AGARIO_BOT_SRC}
        engine/Engine.hpp
        engine/GameState.hpp
        core/settings.hpp)

set(AGARIO_RENDERING_SRC
        # /usr/include/glad/glad.h
        rendering/types.hpp
        rendering/platform.hpp
        rendering/renderer.hpp
        rendering/shader.hpp
        rendering/window.hpp
        rendering/FrameBufferObject.hpp)

set (UTILS
        utils/json.hpp
        utils/json_fwd.hpp
        utils/collision_detection.hpp
        utils/random.hpp
        utils/structures.hpp)

set(AGARIO_SRC ${AGARIO_CORE_SRC} ${AGARIO_ENGINE_SRC})

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
#find_library(GLFW REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(client engine ${OPENGL_INCLUDE_DIR})

set(AGARIO_CLIENT_SRC
        ${AGARIO_BOT_SRC}
        ${AGARIO_RENDERING_SRC}
        ${UTILS}
        core/Entities.hpp
        client/client.hpp
        rendering/renderer.hpp
        rendering/shader.hpp
        )



set(EXT_SOURCE_DIR "../dependencies/glad/src")
set(EXT_INCLUDE_DIR "../dependencies/glad/include")
add_library(glad "${EXT_INCLUDE_DIR}/glad/glad.h" "${EXT_SOURCE_DIR}/glad.c")

# Add the -fPIC flag to ensure position-independent code
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(glad PUBLIC ${EXT_INCLUDE_DIR})

add_executable(client ${AGARIO_CLIENT_SRC} ${AGARIO_BOT_SRC}  client/main.cpp)
# target_link_libraries(client ${OPENGL_LIBRARIES} OpenGL::EGL glad glfw glm)
if(APPLE)
    target_link_libraries(client ${OPENGL_LIBRARIES} glad glfw glm)
else()
    target_link_libraries(client ${OPENGL_LIBRARIES} OpenGL::EGL glad glfw glm)
endif()


include_directories(server)
set(AGARIO_SERVER_SRC
        server/main.cpp)
add_executable(server ${AGARIO_SERVER_SRC})


# ============================================================
# Bot Benchmarking
# ============================================================

add_executable(bot-compare bots/benchmark.cpp)
target_include_directories(bot-compare PRIVATE "..")
target_link_libraries(bot-compare util)

# ============================================================
# Testing
# ============================================================

include_directories(test)
set(TEST_SRC
        test/test-core.hpp
        test/test-entities.hpp
        test/test-engine.hpp
        test/renderable.hpp
        test/main.cpp)

find_package(GTest)
if (GTEST_FOUND)
    #set(TEST_FLAGS "-g -O0 -Wall -Wextra -pedantic")

    include_directories(test ${GTEST_INCLUDE_DIRS})

    add_executable(test-engine ${TEST_SRC} ${AGARIO_SRC})
    target_link_libraries(test-engine pthread gtest)

    find_package(OpenGL REQUIRED)
    if (OpenGL_FOUND)
        add_executable(test-engine-renderable ${TEST_SRC} ${AGARIO_SRC})
        target_include_directories(test-engine-renderable PRIVATE ${OPENGL_INCLUDE_DIR})

        # target_link_libraries(test-engine-renderable pthread gtest ${OPENGL_LIBRARIES} OpenGL::EGL glad glm glfw)
        if(APPLE)
            target_link_libraries(test-engine-renderable pthread gtest ${OPENGL_LIBRARIES} glad glm glfw)
        else()
            target_link_libraries(test-engine-renderable pthread gtest ${OPENGL_LIBRARIES} OpenGL::EGL glad glm glfw)
        endif()
        target_compile_definitions(test-engine-renderable PUBLIC RENDERABLE)

    else()
        message("OpenGL not found")
    endif()

else()
    message("Google Test not found")
endif()
