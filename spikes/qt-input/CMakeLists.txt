cmake_minimum_required(VERSION 3.22)
project(WorkbenchInputProbe LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup()
qt_add_executable(workbench-input-probe main.cpp)
qt_add_resources(workbench-input-probe "probe" PREFIX "/" FILES Main.qml)
target_link_libraries(workbench-input-probe PRIVATE Qt6::Quick Qt6::QuickControls2)
set_target_properties(workbench-input-probe PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.input-probe"
    MACOSX_BUNDLE_BUNDLE_NAME "Workbench Input Probe")

option(WORKBENCH_TERMINAL_PROBE "Build the isolated runtime terminal host" OFF)
if(WORKBENCH_TERMINAL_PROBE)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Network QuickDialogs2)
    set(VT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.local/runtime-vt")
    if(NOT EXISTS "${VT_ROOT}/lib/libghostty-vt.a")
        message(FATAL_ERROR "Run just runtime-build to prepare the pinned Ghostty VT library")
    endif()
    qt_add_executable(workbench-text-bench text_bench.cpp terminal_bench_fixture.cpp terminal_text.cpp)
    target_compile_definitions(workbench-text-bench PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-text-bench PRIVATE -Wall -Wextra)
    target_include_directories(workbench-text-bench PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-text-bench PRIVATE Qt6::Gui "${VT_ROOT}/lib/libghostty-vt.a")
    qt_add_resources(workbench-text-bench "bench_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    set_target_properties(workbench-text-bench PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.raster-benchmark"
        MACOSX_BUNDLE_BUNDLE_NAME "Workbench Raster Benchmark")
    qt_add_executable(workbench-scene-bench scene_bench.cpp terminal_bench_fixture.cpp terminal_text.cpp)
    target_compile_definitions(workbench-scene-bench PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-scene-bench PRIVATE -Wall -Wextra)
    target_include_directories(workbench-scene-bench PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-scene-bench PRIVATE Qt6::Quick "${VT_ROOT}/lib/libghostty-vt.a")
    qt_add_resources(workbench-scene-bench "scene_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    set_target_properties(workbench-scene-bench PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.scene-benchmark"
        MACOSX_BUNDLE_BUNDLE_NAME "Workbench Scene Benchmark")
    qt_add_executable(workbench-text-probe text_main.cpp terminal_text.cpp terminal_text_test.cpp)
    target_compile_definitions(workbench-text-probe PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-text-probe PRIVATE -Wall -Wextra)
    target_include_directories(workbench-text-probe PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-text-probe PRIVATE Qt6::Gui "${VT_ROOT}/lib/libghostty-vt.a")
    qt_add_resources(workbench-text-probe "text_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    set_target_properties(workbench-text-probe PROPERTIES MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.text-probe"
        MACOSX_BUNDLE_BUNDLE_NAME "Workbench Text Proof")
    qt_add_executable(workbench-terminal-probe terminal_main.cpp terminal_item.cpp terminal_item.h terminal_search.cpp terminal_search.h search_bridge.c theme_bridge.c terminal_accessibility.cpp terminal_accessibility.h terminal_accessibility_test.cpp terminal_composition.cpp terminal_composition_test.cpp terminal_composition.h terminal_text.cpp terminal_text.h)
    qt_add_resources(workbench-terminal-probe "terminal" PREFIX "/" FILES Terminal.qml Pair.qml)
    # Ghostty formatter structs have an emit field; avoid Qt keyword macros.
    target_compile_definitions(workbench-terminal-probe PRIVATE QT_NO_KEYWORDS)
    qt_add_resources(workbench-terminal-probe "terminal_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    target_compile_options(workbench-terminal-probe PRIVATE -Wall -Wextra)
    target_include_directories(workbench-terminal-probe PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-terminal-probe PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network "${VT_ROOT}/lib/libghostty-vt.a")
    set_target_properties(workbench-terminal-probe PROPERTIES MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.terminal-probe"
        MACOSX_BUNDLE_BUNDLE_NAME "Workbench Terminal Probe")
    qt_add_executable(workbench-ui-study ui_display_report.cpp ui_display_report.h ui_catalog_feed.cpp ui_catalog_feed.h ui_main.cpp ui_view_state.cpp ui_view_state.h ui_shortcuts.cpp ui_shortcuts.h shortcut_bridge.c ui_fonts.cpp ui_fonts.h font_bridge.c ui_lifecycle.cpp ui_lifecycle.h chrome_icon.cpp chrome_icon.h ui_session.cpp ui_session.h runtime_facts.cpp runtime_facts.h ui_themes.cpp ui_themes.h theme_store.cpp theme_store.h terminal_item.cpp terminal_item.h terminal_search.cpp terminal_search.h search_bridge.c theme_bridge.c
        terminal_accessibility.cpp terminal_composition.cpp terminal_text.cpp)
    target_compile_definitions(workbench-ui-study PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-ui-study PRIVATE -Wall -Wextra)
    target_include_directories(workbench-ui-study PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-ui-study PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Network "${VT_ROOT}/lib/libghostty-vt.a")
    qt_add_resources(workbench-ui-study "ui" PREFIX "/" FILES Ui.qml NavigationPanel.qml ShortcutSettings.qml FontSettings.qml CommandPalette.qml WorkbenchActions.qml ThemePicker.qml ThemeImport.qml ChromeButton.qml ChromeMenu.qml ChromeMenuItem.qml TerminalSearch.qml)
    qt_add_resources(workbench-ui-study "ui_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/jetbrains-mono/JetBrainsMono-Regular.ttf" "../../fonts/jetbrains-mono/OFL.txt" "../../fonts/adwaita-sans/AdwaitaSans-Regular.ttf" "../../fonts/adwaita-sans/OFL.txt" "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    set_target_properties(workbench-ui-study PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.ui-study"
        MACOSX_BUNDLE_BUNDLE_NAME "Terminal Workbench UI Study")
    qt_add_executable(workbench-catalog-test ui_catalog_feed_test.cpp ui_catalog_feed.cpp ui_catalog_feed.h)
    target_compile_definitions(workbench-catalog-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-catalog-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-catalog-test PRIVATE Qt6::Network)
    set_target_properties(workbench-catalog-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-view-state-test ui_view_state_test.cpp ui_view_state.cpp ui_view_state.h)
    target_compile_definitions(workbench-view-state-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-view-state-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-view-state-test PRIVATE Qt6::Core)
    set_target_properties(workbench-view-state-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-search-test terminal_search_test.cpp terminal_search.cpp terminal_search.h search_bridge.c)
    target_compile_definitions(workbench-search-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-search-test PRIVATE -Wall -Wextra)
    target_include_directories(workbench-search-test PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-search-test PRIVATE Qt6::Core "${VT_ROOT}/lib/libghostty-vt.a")
    set_target_properties(workbench-search-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-scroll-test terminal_scroll_test.cpp)
    target_compile_options(workbench-scroll-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-scroll-test PRIVATE Qt6::Core)
    set_target_properties(workbench-scroll-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-shortcuts-test ui_shortcuts_test.cpp ui_shortcuts.cpp ui_shortcuts.h shortcut_bridge.c)
    target_compile_definitions(workbench-shortcuts-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-shortcuts-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-shortcuts-test PRIVATE Qt6::Gui)
    set_target_properties(workbench-shortcuts-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-fonts-test ui_fonts_test.cpp ui_fonts.cpp ui_fonts.h font_bridge.c)
    target_compile_definitions(workbench-fonts-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-fonts-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-fonts-test PRIVATE Qt6::Core)
    set_target_properties(workbench-fonts-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-lifecycle-test ui_lifecycle_test.cpp ui_lifecycle.cpp ui_lifecycle.h)
    target_compile_definitions(workbench-lifecycle-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-lifecycle-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-lifecycle-test PRIVATE Qt6::Core Qt6::Network)
    set_target_properties(workbench-lifecycle-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-runtime-facts-test runtime_facts_test.cpp runtime_facts.cpp runtime_facts.h)
    target_compile_definitions(workbench-runtime-facts-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-runtime-facts-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-runtime-facts-test PRIVATE Qt6::Core Qt6::Network)
    set_target_properties(workbench-runtime-facts-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-theme-import-test ui_themes_test.cpp ui_themes.cpp ui_themes.h theme_store.cpp theme_store.h theme_bridge.c)
    target_compile_definitions(workbench-theme-import-test PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-theme-import-test PRIVATE -Wall -Wextra)
    target_link_libraries(workbench-theme-import-test PRIVATE Qt6::Gui Qt6::Network)
    set_target_properties(workbench-theme-import-test PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE FALSE)
    qt_add_executable(workbench-live-bench live_bench.cpp terminal_item.cpp terminal_item.h terminal_search.cpp terminal_search.h search_bridge.c theme_bridge.c
        terminal_accessibility.cpp terminal_accessibility.h terminal_composition.cpp terminal_composition.h
        terminal_text.cpp terminal_text.h)
    target_compile_definitions(workbench-live-bench PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-live-bench PRIVATE -Wall -Wextra)
    target_include_directories(workbench-live-bench PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-live-bench PRIVATE Qt6::Quick Qt6::Network "${VT_ROOT}/lib/libghostty-vt.a")
    qt_add_resources(workbench-live-bench "live_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    set_target_properties(workbench-live-bench PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.live-benchmark"
        MACOSX_BUNDLE_BUNDLE_NAME "Workbench Live Terminal Benchmark")
endif()


# Finder bootstrap has no Qt dependency; it can report a missing Python install.
if(APPLE AND WORKBENCH_TERMINAL_PROBE)
    add_executable(workbench-preview-bootstrap EXCLUDE_FROM_ALL ../../native/macos/preview_launcher.c)
    target_compile_options(workbench-preview-bootstrap PRIVATE -Wall -Wextra -Werror)
    set_target_properties(workbench-preview-bootstrap PROPERTIES MACOSX_BUNDLE FALSE)
endif()


# Select deployment tools from the same Qt installation as the viewer.
if(TARGET Qt6::qtpaths)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/workbench-qt-tools.txt"
        CONTENT "$<TARGET_FILE:Qt6::qtpaths>\n${Qt6_VERSION}\n")
endif()
