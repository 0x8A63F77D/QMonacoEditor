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

# Escape hatch from the build-time npm requirement: point this at a frontend
# bundle that was built elsewhere (a release asset, or a colleague's build) and
# the Monaco frontend is taken from it verbatim -- no Node.js/npm is looked for
# or invoked. Empty (the default) keeps the npm/vite build. See src/CMakeLists.txt.
#
# The condition distinguishes the two ways this name can already be set, which
# want opposite treatment:
#
#  - a *normal* variable from consumer code, i.e.
#        set(QMONACO_PREBUILT_RESOURCES "...")   # before FetchContent_MakeAvailable
#    the idiomatic way to configure a subproject. Declaring the cache entry
#    would delete it -- set(CACHE) removes the normal variable of the same name
#    -- and quietly fall back to the npm build. So: leave it alone.
#
#  - a *cache* entry, including the UNINITIALIZED one that a plain
#    -DQMONACO_PREBUILT_RESOURCES=... creates. Declaring it here is what gives
#    that entry its PATH type, and typing it is what converts a relative
#    command-line value into an absolute one. So: declare it.
if(NOT DEFINED QMONACO_PREBUILT_RESOURCES OR DEFINED CACHE{QMONACO_PREBUILT_RESOURCES})
    set(QMONACO_PREBUILT_RESOURCES "" CACHE PATH
        "Prebuilt Monaco frontend bundle: absolute path to a directory holding index.html, or to an archive of one")
endif()

add_subdirectory(src)

if(QMONACO_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(QMONACO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
