# --- Frontend resources ---
# The Monaco frontend reaches the library as Qt resources. Where those files
# come from is the one choice made here: a prebuilt bundle handed to us, or a
# local npm/vite build. Everything downstream (qrc generation, rcc) is shared.
set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/qmonacoeditor.qrc")
set(QRC_CPP "${CMAKE_CURRENT_BINARY_DIR}/qrc_qmonacoeditor.cpp")
set(GENERATE_QRC_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/generate_qrc.cmake")

find_program(QT_RCC_EXECUTABLE
    NAMES rcc
    HINTS "${Qt6_DIR}/../../../bin"
          "${Qt6_DIR}/../../.."
    REQUIRED
)

# --- Script to generate qrc at build time ---
file(WRITE "${GENERATE_QRC_SCRIPT}" [=[
file(GLOB_RECURSE RESOURCE_FILES "${RESOURCES_DIR}/*")
set(QRC_CONTENT "<RCC>\n  <qresource prefix=\"/qmonacoeditor\">\n")
foreach(FILE ${RESOURCE_FILES})
    file(RELATIVE_PATH REL_PATH "${RESOURCES_DIR}" "${FILE}")
    string(APPEND QRC_CONTENT "    <file alias=\"${REL_PATH}\">${FILE}</file>\n")
endforeach()
string(APPEND QRC_CONTENT "  </qresource>\n</RCC>\n")
# Written via a temporary and configure_file, which replaces the destination
# only when the content actually differs. That is what lets this script be run
# unconditionally (see step 2) without re-running rcc and recompiling the
# frontend on every build.
file(WRITE "${QRC_FILE}.tmp" "${QRC_CONTENT}")
configure_file("${QRC_FILE}.tmp" "${QRC_FILE}" COPYONLY)
file(REMOVE "${QRC_FILE}.tmp")
]=])

# --- Step 1: Obtain the frontend ---
# Both branches must leave the same two variables behind: RESOURCES_DIR (the
# directory whose contents become the qrc payload) and QRC_INPUT_DEPENDS (what
# the qrc must be regenerated after).
if(QMONACO_PREBUILT_RESOURCES)
    # A bundle was handed to us, so nothing here may need Node.js: this branch
    # deliberately contains no find_program(npm) and no vite command. That is
    # the whole point of the option -- consumers without npm can still build.
    file(TO_CMAKE_PATH "${QMONACO_PREBUILT_RESOURCES}" PREBUILT_PATH)

    # Relative values that arrive through the cache (-D..., or a set(CACHE PATH)
    # in consumer code) never reach this check -- CMake absolutises PATH-typed
    # cache entries against the directory cmake was invoked from, which is what
    # the user means anyway. A plain set(QMONACO_PREBUILT_RESOURCES "bundle") is
    # left exactly as written, and what a relative path resolves against there
    # is not something to guess at, so it is refused instead.
    if(NOT IS_ABSOLUTE "${PREBUILT_PATH}")
        message(FATAL_ERROR
            "QMONACO_PREBUILT_RESOURCES is the relative path "
            "'${QMONACO_PREBUILT_RESOURCES}'. Set it to an absolute path.")
    endif()

    if(IS_DIRECTORY "${PREBUILT_PATH}")
        set(RESOURCES_DIR "${PREBUILT_PATH}")
    elseif(EXISTS "${PREBUILT_PATH}")
        # An archive, extracted into the build tree with the destination wiped
        # first so a replaced archive cannot leave stale files behind.
        set(RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/prebuilt-resources")

        # Unpacking is a configure-time act, so a build alone would keep using a
        # previously extracted payload even after the archive was swapped at the
        # same path. Listing the archive here makes such a swap re-run configure,
        # which is what gets it unpacked. (Timestamp-based, as all of CMake's
        # configure dependencies are: replacing an archive with one whose
        # timestamp is *older* than the last configure will not be noticed.)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PREBUILT_PATH}")

        # Keyed on content, so reconfiguring for an unrelated reason does not
        # re-unpack the whole frontend.
        file(SHA256 "${PREBUILT_PATH}" ARCHIVE_HASH)
        if(NOT EXISTS "${RESOURCES_DIR}/index.html"
           OR NOT "${QMONACO_EXTRACTED_ARCHIVE}" STREQUAL "${ARCHIVE_HASH}")
            file(REMOVE_RECURSE "${RESOURCES_DIR}")
            file(ARCHIVE_EXTRACT INPUT "${PREBUILT_PATH}" DESTINATION "${RESOURCES_DIR}")
            set(QMONACO_EXTRACTED_ARCHIVE "${ARCHIVE_HASH}" CACHE INTERNAL
                "SHA256 of the archive currently extracted into prebuilt-resources")
        endif()
    else()
        message(FATAL_ERROR
            "QMONACO_PREBUILT_RESOURCES points at '${PREBUILT_PATH}', which "
            "does not exist.")
    endif()

    # index.html is the entry point QMonacoEditor loads at runtime; without it
    # the bundle is not a frontend build, whatever else it contains.
    if(NOT EXISTS "${RESOURCES_DIR}/index.html")
        message(FATAL_ERROR
            "QMONACO_PREBUILT_RESOURCES: no index.html in '${RESOURCES_DIR}'. "
            "Expected a QMonacoEditor frontend bundle -- either the directory "
            "produced by 'npm run build' in web/ (this repository's resources/ "
            "directory), or the qmonacoeditor-resources-<version>.zip asset of "
            "a GitHub release. Archive contents must sit at the archive root, "
            "not inside a wrapping directory.")
    endif()

    # Deliberately no file dependencies. The payload belongs to the consumer and
    # can change under a live build tree; every attempt to detect that at
    # configure time (a file list, CONFIGURE_DEPENDS, a content hash) had its own
    # blind spot, and a stale file list additionally breaks the build outright
    # once a globbed asset is renamed away. So the qrc rule below is instead made
    # to run on every build and read the payload afresh -- see step 2.
    set(QRC_INPUT_DEPENDS "")
    set(QRC_ALWAYS_REGENERATE "${CMAKE_CURRENT_BINARY_DIR}/regenerate_qrc_always")

    message(STATUS "QMonacoEditor: using prebuilt frontend from ${RESOURCES_DIR} (npm not required)")
else()
    # CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: when consumed via
    # add_subdirectory/FetchContent the latter is the consumer's root, which
    # would break the frontend paths.
    set(WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../web")
    set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../resources")
    set(VITE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/vite_build.stamp")

    find_program(NPM_EXECUTABLE npm REQUIRED)

    # Which npm installation is used stays entirely up to find_program (PATH order,
    # platform rules, a user-supplied -DNPM_EXECUTABLE). The single invariant this
    # block enforces is how that one installation is *spelled* in the build command:
    # it must carry the .cmd suffix when it is a batch shim.
    #
    # Why: the Visual Studio (MSBuild) generator writes all commands of one
    # add_custom_command into a single batch file, and prefixes a command with
    # "call " only when it recognises a .bat/.cmd extension. Given the
    # extension-less "C:/Program Files/nodejs/npm" shim there is no "call", so
    # cmd.exe resolves it via PATHEXT to npm.cmd and *chains* to it: control never
    # returns, every following command is skipped, and the batch still exits 0 --
    # leaving vite_build.stamp, the .qrc and qrc_*.cpp unbuilt (later C1083).
    #
    # Appending .cmd to the path already found keeps the chosen installation intact
    # (no second search, no cache rewrite, so a build tree configured before this
    # fix is corrected on reconfigure too). CMAKE_HOST_WIN32, not WIN32: npm runs on
    # the machine performing the build, and the two differ when cross-compiling.
    set(NPM_COMMAND "${NPM_EXECUTABLE}")
    if(CMAKE_HOST_WIN32 AND EXISTS "${NPM_EXECUTABLE}.cmd")
        set(NPM_COMMAND "${NPM_EXECUTABLE}.cmd")
    endif()

    add_custom_command(
        OUTPUT "${VITE_STAMP}"
        COMMAND "${NPM_COMMAND}" install
        COMMAND "${NPM_COMMAND}" run build
        COMMAND ${CMAKE_COMMAND} -E touch "${VITE_STAMP}"
        WORKING_DIRECTORY "${WEB_DIR}"
        DEPENDS "${WEB_DIR}/package.json"
                "${WEB_DIR}/vite.config.js"
                "${WEB_DIR}/index.html"
                "${WEB_DIR}/src/index.ts"
        COMMENT "Building Monaco Editor frontend resources"
    )

    set(QRC_INPUT_DEPENDS "${VITE_STAMP}")
endif()

# --- Step 2: Generate qrc from the frontend files ---
# On the npm path the payload is produced by the build itself, so depending on
# the vite stamp is enough. On the prebuilt path it is not: the payload is owned
# by the consumer, who may swap it under an already-configured build tree, and
# extraction restores whatever timestamps the archive carried. Rather than keep
# guessing when that happened, this rule declares an extra output that is never
# written (SYMBOLIC, so CMake knows not to expect it). The rule is therefore
# permanently out of date and re-reads the payload on every build; the script
# replaces the qrc only when the content differs, so rcc and the recompile below
# still happen exactly when the frontend really changed.
if(QMONACO_PREBUILT_RESOURCES)
    set_source_files_properties("${QRC_ALWAYS_REGENERATE}" PROPERTIES SYMBOLIC TRUE)
endif()

add_custom_command(
    OUTPUT "${QRC_FILE}" ${QRC_ALWAYS_REGENERATE}
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCES_DIR="${RESOURCES_DIR}"
        -DQRC_FILE="${QRC_FILE}"
        -P "${GENERATE_QRC_SCRIPT}"
    DEPENDS ${QRC_INPUT_DEPENDS}
    COMMENT "Generating Qt resource file from frontend build output"
)

# --- Step 3: Compile qrc with rcc ---
add_custom_command(
    OUTPUT "${QRC_CPP}"
    COMMAND "${QT_RCC_EXECUTABLE}" --no-zstd --name qmonacoeditor -o "${QRC_CPP}" "${QRC_FILE}"
    DEPENDS "${QRC_FILE}"
    COMMENT "Compiling Qt resources with rcc"
)

# --- Library target ---
add_library(qmonacoeditor STATIC
    QMonacoEditor.h
    QMonacoEditor.cpp
    MonacoBridge.h
    MonacoBridge.cpp
    "${QRC_CPP}"
)

target_include_directories(qmonacoeditor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(qmonacoeditor PUBLIC
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)