cmake_minimum_required(VERSION 3.22)
project(WorkbenchInputProbe LANGUAGES CXX)
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
    find_package(Qt6 6.8 REQUIRED COMPONENTS Network)
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
    qt_add_executable(workbench-terminal-probe terminal_main.cpp terminal_item.cpp terminal_item.h terminal_accessibility.cpp terminal_accessibility.h terminal_accessibility_test.cpp terminal_composition.cpp terminal_composition_test.cpp terminal_composition.h terminal_text.cpp terminal_text.h)
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
    qt_add_executable(workbench-ui-study ui_main.cpp terminal_item.cpp terminal_item.h
        terminal_accessibility.cpp terminal_composition.cpp terminal_text.cpp)
    target_compile_definitions(workbench-ui-study PRIVATE QT_NO_KEYWORDS)
    target_compile_options(workbench-ui-study PRIVATE -Wall -Wextra)
    target_include_directories(workbench-ui-study PRIVATE "${VT_ROOT}/include")
    target_link_libraries(workbench-ui-study PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network "${VT_ROOT}/lib/libghostty-vt.a")
    qt_add_resources(workbench-ui-study "ui" PREFIX "/" FILES Ui.qml)
    qt_add_resources(workbench-ui-study "ui_font" PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        FILES "../../fonts/fira-code/FiraCode-Regular.ttf" "../../fonts/fira-code/OFL.txt")
    set_target_properties(workbench-ui-study PROPERTIES EXCLUDE_FROM_ALL TRUE MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.terminal-workbench.ui-study"
        MACOSX_BUNDLE_BUNDLE_NAME "Terminal Workbench UI Study")
    qt_add_executable(workbench-live-bench live_bench.cpp terminal_item.cpp terminal_item.h
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
