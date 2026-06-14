# --- Frontend build via npm/vite ---
set(WEB_DIR "${CMAKE_SOURCE_DIR}/web")
set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")
set(VITE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/vite_build.stamp")
set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/qmonacoeditor.qrc")
set(QRC_CPP "${CMAKE_CURRENT_BINARY_DIR}/qrc_qmonacoeditor.cpp")
set(GENERATE_QRC_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/generate_qrc.cmake")

find_program(NPM_EXECUTABLE npm REQUIRED)
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
file(WRITE "${QRC_FILE}" "${QRC_CONTENT}")
]=])

# --- Step 1: Build frontend ---
add_custom_command(
    OUTPUT "${VITE_STAMP}"
    COMMAND "${NPM_EXECUTABLE}" install
    COMMAND "${NPM_EXECUTABLE}" run build
    COMMAND ${CMAKE_COMMAND} -E touch "${VITE_STAMP}"
    WORKING_DIRECTORY "${WEB_DIR}"
    DEPENDS "${WEB_DIR}/package.json"
            "${WEB_DIR}/vite.config.js"
            "${WEB_DIR}/index.html"
            "${WEB_DIR}/src/index.ts"
    COMMENT "Building Monaco Editor frontend resources"
)

# --- Step 2: Generate qrc from build output ---
add_custom_command(
    OUTPUT "${QRC_FILE}"
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCES_DIR="${RESOURCES_DIR}"
        -DQRC_FILE="${QRC_FILE}"
        -P "${GENERATE_QRC_SCRIPT}"
    DEPENDS "${VITE_STAMP}"
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