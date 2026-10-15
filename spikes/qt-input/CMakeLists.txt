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
