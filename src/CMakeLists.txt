# --- Frontend build via npm/vite ---
set(WEB_DIR "${CMAKE_SOURCE_DIR}/web")
set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")
set(VITE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/vite_build.stamp")

find_program(NPM_EXECUTABLE npm REQUIRED)

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

add_custom_target(web_build DEPENDS "${VITE_STAMP}")

# --- Collect resource files dynamically ---
file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS "${RESOURCES_DIR}/*")

# --- Qt resource file ---
set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/qmonacoeditor.qrc")
file(WRITE "${QRC_FILE}" "<RCC>\n  <qresource prefix=\"/qmonacoeditor\">\n")
foreach(FILE ${RESOURCE_FILES})
    file(RELATIVE_PATH REL_PATH "${RESOURCES_DIR}" "${FILE}")
    file(APPEND "${QRC_FILE}" "    <file alias=\"${REL_PATH}\">${FILE}</file>\n")
endforeach()
file(APPEND "${QRC_FILE}" "  </qresource>\n</RCC>\n")

# --- Library target ---
add_library(qmonacoeditor STATIC
    QMonacoEditor.h
    QMonacoEditor.cpp
    MonacoBridge.h
    MonacoBridge.cpp
    "${QRC_FILE}"
)

add_dependencies(qmonacoeditor web_build)

target_include_directories(qmonacoeditor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(qmonacoeditor PUBLIC
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)
