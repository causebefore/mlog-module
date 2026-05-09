cmake_minimum_required(VERSION 3.16)

project(mlog_module C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MLOG_BUILD_EXAMPLE "Build bare-metal minimal example" ON)

add_library(mlog STATIC
    src/mlog.c
    src/mlog_buf.c
)

target_include_directories(mlog
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

if(MLOG_BUILD_EXAMPLE)
    add_executable(mlog_example
        examples/baremetal_minimal/main.c
        examples/baremetal_minimal/mlog_port_minimal.c
    )

    target_include_directories(mlog_example
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/inc
    )

    target_link_libraries(mlog_example PRIVATE mlog)
endif()
