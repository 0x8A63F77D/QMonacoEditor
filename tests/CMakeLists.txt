find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(qmonacoeditor_tests test_qmonacoeditor.cpp)

target_link_libraries(qmonacoeditor_tests PRIVATE
    qmonacoeditor
    Qt6::Test
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

add_test(NAME qmonacoeditor_tests COMMAND qmonacoeditor_tests)

# --- MonacoBridge unit tests (no WebEngine needed) ---
add_executable(monacobridge_tests test_monacobridge.cpp)

target_link_libraries(monacobridge_tests PRIVATE
    qmonacoeditor
    Qt6::Test
)

add_test(NAME monacobridge_tests COMMAND monacobridge_tests)
