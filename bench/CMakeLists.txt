cmake_minimum_required(VERSION 2.8...3.20)

set(BENCH_SOURCE
        main.cpp)

if(APPLE)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    link_directories(/usr/local/lib)
    include_directories(/usr/local/include)
endif()

add_executable(agario-bench ${BENCH_SOURCE})
target_include_directories(agario-bench PRIVATE "..")
target_link_libraries(agario-bench PRIVATE benchmark pthread)
