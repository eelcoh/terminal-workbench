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
    qt_add_resources(workbench-terminal-probe "terminal" PREFIX "/" FILES Terminal.qml)
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
endif()
