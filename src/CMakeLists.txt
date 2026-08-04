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
# Unpacking a prebuilt archive happens here rather than at configure time,
# because this script runs on every build: replacing the archive is then picked
# up by a plain `cmake --build`, and the decision rests on the archive's content
# rather than on its timestamp (which may move backwards -- cp -p, a restored
# artifact -- leaving a timestamp-based check satisfied).
if(PREBUILT_ARCHIVE)
    file(SHA256 "${PREBUILT_ARCHIVE}" ARCHIVE_HASH)
    set(HASH_FILE "${RESOURCES_DIR}.sha256")
    set(EXTRACTED_HASH "")
    if(EXISTS "${HASH_FILE}")
        file(READ "${HASH_FILE}" EXTRACTED_HASH)
    endif()
    if(NOT EXISTS "${RESOURCES_DIR}/index.html" OR NOT EXTRACTED_HASH STREQUAL ARCHIVE_HASH)
        # Unpacked through execute_process rather than file(ARCHIVE_EXTRACT) for
        # one reason: its diagnostics can be read. Damage is reported on stderr
        # while the exit status stays 0, and it takes more than one form -- a
        # truncated archive stops partway, and a zip whose entry data is corrupt
        # but whose directory is intact unpacks every file with one of them
        # silently wrong ("ZIP bad CRC"). Listing the archive first does not
        # cover the second case, because listing only reads the directory. So
        # any diagnostic at all is treated as a failure.
        # Dropped before the tree is touched, not merely rewritten after: from
        # here until the unpack succeeds the payload matches no archive at all,
        # and if the unpack fails that is where it stays. A stale hash left
        # behind would match again the moment the previous archive was restored,
        # and the unpack would be skipped over the wreckage of the failed one.
        file(REMOVE "${HASH_FILE}")
        file(REMOVE_RECURSE "${RESOURCES_DIR}")
        file(MAKE_DIRECTORY "${RESOURCES_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${PREBUILT_ARCHIVE}"
            WORKING_DIRECTORY "${RESOURCES_DIR}"
            RESULT_VARIABLE UNPACK_RESULT
            ERROR_VARIABLE UNPACK_ERROR
        )
        if(NOT UNPACK_RESULT EQUAL 0 OR NOT UNPACK_ERROR STREQUAL "")
            message(FATAL_ERROR
                "QMonacoEditor: '${PREBUILT_ARCHIVE}' did not unpack cleanly -- "
                "it looks damaged or incompletely downloaded. ${UNPACK_ERROR}")
        endif()

        # Recorded only now: an abort above must leave the previous hash in
        # place, so the next build retries instead of trusting a bad unpack.
        file(WRITE "${HASH_FILE}" "${ARCHIVE_HASH}")
    endif()
endif()

# Validated here rather than only at configure time, because the payload can be
# replaced after configuration: an archive swapped for a malformed one is
# unpacked above, and without this the build would happily emit a qrc with no
# entry point and produce a library whose editors come up blank.
# IS_DIRECTORY as well as EXISTS: a payload may contain a *directory* named
# index.html, which passes an existence test while leaving the widget navigating
# to a directory instead of the entry document -- a blank editor, not an error.
if(NOT EXISTS "${RESOURCES_DIR}/index.html" OR IS_DIRECTORY "${RESOURCES_DIR}/index.html")
    message(FATAL_ERROR
        "QMonacoEditor: no index.html file in '${RESOURCES_DIR}'. "
        "Expected a QMonacoEditor frontend bundle -- either the directory "
        "produced by 'npm run build' in web/ (this repository's resources/ "
        "directory), or the qmonacoeditor-resources-<version>.zip asset of a "
        "GitHub release. Archive contents must sit at the archive root, not "
        "inside a wrapping directory.")
endif()

file(GLOB_RECURSE RESOURCE_FILES "${RESOURCES_DIR}/*")

# The qrc is written below only when its text differs from the existing one,
# which is what keeps rcc off the critical path of every build. That makes the
# text the sole signal that the payload changed -- and file names alone are not
# that signal: an edit confined to a stable-named file (index.html) changes what
# must be embedded while leaving every path identical. A fingerprint of the
# contents is therefore part of the document, so "same text" really does mean
# "same bytes to embed".
set(PAYLOAD_FINGERPRINT "")
foreach(FILE IN LISTS RESOURCE_FILES)
    # A ";" anywhere in the payload's path is unrecoverable here: file(GLOB) hands
    # back a plain CMake list, and a semicolon inside a name is indistinguishable
    # from the separator between two names by the time we see it -- iterating IN
    # LISTS does not help, because the damage happened in the glob. The symptom
    # would otherwise be file(SHA256) failing on a truncated path, so say what is
    # actually wrong instead.
    if(NOT EXISTS "${FILE}")
        message(FATAL_ERROR
            "QMonacoEditor: '${FILE}' does not exist. This usually means the "
            "bundle's path or one of its file names contains a ';', which CMake "
            "cannot tell apart from a list separator. Move the bundle to a path "
            "without semicolons.")
    endif()
    file(SHA256 "${FILE}" FILE_HASH)
    string(APPEND PAYLOAD_FINGERPRINT "${FILE}:${FILE_HASH}\n")
endforeach()
string(SHA256 PAYLOAD_HASH "${PAYLOAD_FINGERPRINT}")

set(QRC_CONTENT "<RCC>\n  <!-- payload ${PAYLOAD_HASH} -->\n  <qresource prefix=\"/qmonacoeditor\">\n")
foreach(FILE IN LISTS RESOURCE_FILES)
    file(RELATIVE_PATH REL_PATH "${RESOURCES_DIR}" "${FILE}")
    # A prebuilt bundle may sit anywhere the consumer likes, including a path
    # holding XML metacharacters (D:/R&D/bundle); left as-is they make rcc
    # reject the qrc as malformed. "&" is replaced first, so that the entities
    # introduced afterwards are not escaped a second time.
    set(ALIAS_XML "${REL_PATH}")
    set(PATH_XML "${FILE}")
    string(REPLACE "&" "&amp;" ALIAS_XML "${ALIAS_XML}")
    string(REPLACE "<" "&lt;" ALIAS_XML "${ALIAS_XML}")
    string(REPLACE ">" "&gt;" ALIAS_XML "${ALIAS_XML}")
    string(REPLACE "\"" "&quot;" ALIAS_XML "${ALIAS_XML}")
    string(REPLACE "&" "&amp;" PATH_XML "${PATH_XML}")
    string(REPLACE "<" "&lt;" PATH_XML "${PATH_XML}")
    string(REPLACE ">" "&gt;" PATH_XML "${PATH_XML}")
    string(APPEND QRC_CONTENT "    <file alias=\"${ALIAS_XML}\">${PATH_XML}</file>\n")
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
#
# Cleared first, not merely assigned in the branch that needs them: under
# add_subdirectory/FetchContent this scope inherits the parent's variables, so
# an unrelated variable of the same name in the consumer's project would
# otherwise be read here as ours. For the archive that is not just wrong but
# destructive -- the generator wipes RESOURCES_DIR and unpacks whatever it was
# handed, over a payload that may be the consumer's own bundle directory.
set(QMONACO_PREBUILT_ARCHIVE "")
set(QMONACO_QRC_ALWAYS_REGENERATE "")

if(QMONACO_PREBUILT_RESOURCES)
    # A bundle was handed to us, so nothing here may need Node.js: this branch
    # deliberately contains no find_program(npm) and no vite command. That is
    # the whole point of the option -- consumers without npm can still build.
    # Refused up front, because a semicolon cannot survive being carried in a
    # CMake variable: the value is already a two-element list by the time it
    # arrives here, and every later step -- the cache entry, the -D handed to the
    # generator, the glob of the payload -- would compound the confusion. Saying
    # so plainly beats a downstream "does not exist" naming a path nobody passed.
    if(QMONACO_PREBUILT_RESOURCES MATCHES ";")
        message(FATAL_ERROR
            "QMONACO_PREBUILT_RESOURCES contains a ';', which CMake cannot tell "
            "apart from a list separator: '${QMONACO_PREBUILT_RESOURCES}'. Move "
            "the bundle to a path without semicolons.")
    endif()

    # Not file(TO_CMAKE_PATH): that reads its input as a native *search path* and
    # splits it on the platform's separator, so a perfectly valid POSIX bundle at
    # /tmp/bundle:v1 would come apart into a two-element list and then fail to
    # exist. Only Windows needs any conversion, and only there is a backslash
    # guaranteed not to be part of a filename.
    set(PREBUILT_PATH "${QMONACO_PREBUILT_RESOURCES}")
    if(CMAKE_HOST_WIN32)
        string(REPLACE "\\" "/" PREBUILT_PATH "${PREBUILT_PATH}")
    endif()

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
        set(RESOURCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/prebuilt-resources")
        set(QMONACO_PREBUILT_ARCHIVE "${PREBUILT_PATH}")
    else()
        message(FATAL_ERROR
            "QMONACO_PREBUILT_RESOURCES points at '${PREBUILT_PATH}', which "
            "does not exist.")
    endif()

    # Run once now what the build runs every time: unpack the archive if there is
    # one, and validate the payload. Failing at configuration is what turns a bad
    # bundle into an immediate, explained error rather than a library that builds
    # cleanly and shows a blank editor. The script owns the check, so it is
    # enforced on every later build too, when the payload may have been replaced.
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            "-DRESOURCES_DIR=${RESOURCES_DIR}"
            "-DQRC_FILE=${QRC_FILE}"
            "-DPREBUILT_ARCHIVE=${QMONACO_PREBUILT_ARCHIVE}"
            -P "${GENERATE_QRC_SCRIPT}"
        RESULT_VARIABLE PREPARE_RESULT
    )
    if(NOT PREPARE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "QMONACO_PREBUILT_RESOURCES: '${PREBUILT_PATH}' is not a usable "
            "frontend bundle -- see the message above.")
    endif()

    # Deliberately no file dependencies. The payload belongs to the consumer and
    # can change under a live build tree; every attempt to detect that at
    # configure time (a file list, CONFIGURE_DEPENDS, a content hash) had its own
    # blind spot, and a stale file list additionally breaks the build outright
    # once a globbed asset is renamed away. So the qrc rule below is instead made
    # to run on every build and read the payload afresh -- see step 2.
    set(QRC_INPUT_DEPENDS "")
    set(QMONACO_QRC_ALWAYS_REGENERATE "${CMAKE_CURRENT_BINARY_DIR}/regenerate_qrc_always")

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
    set_source_files_properties("${QMONACO_QRC_ALWAYS_REGENERATE}" PROPERTIES SYMBOLIC TRUE)
endif()

add_custom_command(
    OUTPUT "${QRC_FILE}" ${QMONACO_QRC_ALWAYS_REGENERATE}
    COMMAND ${CMAKE_COMMAND}
        "-DRESOURCES_DIR=${RESOURCES_DIR}"
        "-DQRC_FILE=${QRC_FILE}"
        "-DPREBUILT_ARCHIVE=${QMONACO_PREBUILT_ARCHIVE}"
        -P "${GENERATE_QRC_SCRIPT}"
    DEPENDS ${QRC_INPUT_DEPENDS}
    # VERBATIM, so CMake escapes these arguments for the native shell instead of
    # the quoting being hand-rolled: a bundle path may hold characters the shell
    # would otherwise act on (D:/R&D/bundle.zip, where "&" ends the command).
    VERBATIM
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