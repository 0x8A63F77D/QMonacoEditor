cmake_minimum_required(VERSION 3.19)
project(QMonacoEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets WebChannel)

option(QMONACO_BUILD_TESTS "Build QMonacoEditor tests" ON)

add_subdirectory(src)
add_subdirectory(example)

if(QMONACO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
