cmake_minimum_required(VERSION 3.5)
project (AvarioEnv)

set(CMAKE_CXX_STANDARD 17)

include_directories("." "..")
add_subdirectory(pybind11)

IF(APPLE)
    # Fix linking on MacOS 10.14+. See https://stackoverflow.com/questions/54068035
    link_directories(/usr/local/lib)
    include_directories(/usr/local/include)
ENDIF()

# Allow for making
option(RENDERABLE "Make environments renderable" ON) # Disabled by default
if (RENDERABLE)
    message("Renderable")
    add_definitions(-DRENDERABLE)


endif ()



# Allow for USE_EGL
option(USE_EGL "Make environments headlessly renderable" ON) # Disabled by default
if (USE_EGL)
    message("Headlessly renderable")
    add_definitions(-DUSE_EGL)
endif ()



set(AGARIO_ENVS_SOURCE
        envs/BaseEnvironment.hpp
        envs/GridEnvironment.hpp)

set(AGARIO_SCREEN_ENV_SOURCE
        envs/BaseEnvironment.hpp
        envs/ScreenEnvironment.hpp)

# make including the screen environment an option
option(INCLUDE_SCREEN_ENV "Compile Screen Environment" ON)
if (INCLUDE_SCREEN_ENV)

    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)

endif()


if (INCLUDE_SCREEN_ENV AND OpenGL_FOUND)
    message("Including ScreenEnvironment in compilation")
    # define the preprocessor macro to include the relevant code
    add_definitions(-DINCLUDE_SCREEN_ENV)
    find_package(glfw3 3.3 REQUIRED)


    include_directories(${GLM_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${GLFW_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})

    #NEW
    # link_directories(${GLEW_LIBRARY_DIRS})

    add_definitions(${GLM_DEFINITIONS})

    pybind11_add_module(agarcl bindings.cpp renderable.hpp
            ${AGARIO_ENVS_SOURCE}
            ${AGARIO_SCREEN_ENV_SOURCE})

    target_include_directories(agarcl PRIVATE "..")
    target_include_directories(agarcl  PRIVATE ${OPENGL_INCLUDE_DIR} ${GLM_INCLUDE_DIRS})
    target_link_libraries(agarcl PUBLIC ${OPENGL_LIBRARIES} glad glm glfw OpenGL::EGL)

else()

    # standard compilation with Grid environments only
    pybind11_add_module(agarcl bindings.cpp renderable.hpp
            ${AGARIO_ENVS_SOURCE})

    target_include_directories(agarcl PRIVATE "..")
    target_include_directories(agarcl  PRIVATE ${OPENGL_INCLUDE_DIR} ${GLM_INCLUDE_DIRS})
    target_link_libraries(agarcl PUBLIC ${OPENGL_LIBRARIES} glad glm glfw OpenGL::EGL)

    target_compile_options(agarcl PUBLIC -fsized-deallocation)

endif()


# ============================================================
# Testing
# ============================================================

find_package(GTest)
if (GTEST_FOUND)

    set(TEST_SRC
            test/main.cpp
            test/grid-env-test.hpp)


    add_executable(test-envs ${TEST_SRC} ${AGARIO_GRID_ENV_SOURCE})
    target_include_directories(test-envs PUBLIC ".." ${GTEST_INDLUCE_DIRS})
    target_link_libraries(test-envs glad glfw OpenGL::EGL gtest pthread ${OPENGL_LIBRARIES})

else()
    message("Google Test not found")
endif()
