cmake_minimum_required(VERSION 3.19)
project(QMonacoEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets WebChannel)

# Tests and the example require extra Qt components (Qt6::Test) and are of no
# use to consumers pulling this in via add_subdirectory/FetchContent, so they
# default to ON only when this is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(QMONACO_MASTER_PROJECT ON)
else()
    set(QMONACO_MASTER_PROJECT OFF)
endif()

option(QMONACO_BUILD_TESTS "Build QMonacoEditor tests" ${QMONACO_MASTER_PROJECT})
option(QMONACO_BUILD_EXAMPLE "Build QMonacoEditor example app" ${QMONACO_MASTER_PROJECT})

add_subdirectory(src)

if(QMONACO_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(QMONACO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
