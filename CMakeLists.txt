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
# Two rules have to hold at once here, and they pull against each other:
#
#  1. The cache entry must always be declared with type PATH. Typing it is what
#     turns a relative -DQMONACO_PREBUILT_RESOURCES=... into an absolute path
#     (a bare -D creates an UNINITIALIZED entry, which is never converted), and
#     it is what makes the option visible in cmake-gui.
#  2. Declaring it must never change which bundle is used. set(CACHE) deletes a
#     normal variable of the same name -- CMP0126's OLD behaviour, which a 3.19
#     baseline selects -- and a consumer's
#         set(QMONACO_PREBUILT_RESOURCES "...")  # before MakeAvailable
#     is exactly such a normal variable. Losing it would silently swap in the
#     cache value, or fall back to the npm build.
#
# So the declaration is unconditional, and the normal binding is captured before
# it and restored after, leaving that binding its usual precedence over the
# cache. A normal variable is detectable only by its value differing from the
# cache entry's -- when they agree there is nothing to preserve anyway.
set(QMONACO_REQUESTED_BUNDLE "${QMONACO_PREBUILT_RESOURCES}")
set(QMONACO_BUNDLE_FROM_NORMAL_VAR FALSE)
if(DEFINED QMONACO_PREBUILT_RESOURCES
   AND (NOT DEFINED CACHE{QMONACO_PREBUILT_RESOURCES}
        OR NOT "${QMONACO_REQUESTED_BUNDLE}" STREQUAL "$CACHE{QMONACO_PREBUILT_RESOURCES}"))
    set(QMONACO_BUNDLE_FROM_NORMAL_VAR TRUE)
endif()

set(QMONACO_PREBUILT_RESOURCES "" CACHE PATH
    "Prebuilt Monaco frontend bundle: absolute path to a directory holding index.html, or to an archive of one")

if(QMONACO_BUNDLE_FROM_NORMAL_VAR)
    set(QMONACO_PREBUILT_RESOURCES "${QMONACO_REQUESTED_BUNDLE}")
endif()
unset(QMONACO_REQUESTED_BUNDLE)
unset(QMONACO_BUNDLE_FROM_NORMAL_VAR)

add_subdirectory(src)

if(QMONACO_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(QMONACO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
