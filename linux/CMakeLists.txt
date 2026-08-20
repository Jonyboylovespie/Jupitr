cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" JUPITR_VERSION LIMIT_COUNT 1)
project(Jupitr VERSION ${JUPITR_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(CTest)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core DBus Gui Widgets Network Svg)
find_path(XCB_INCLUDE_DIR xcb/xcb.h REQUIRED)
find_library(XCB_LIBRARY xcb PATH_SUFFIXES lib64 lib)
if(NOT XCB_LIBRARY)
    find_file(XCB_LIBRARY libxcb.so.1 PATHS /usr/lib64 /lib64 /usr/lib /lib REQUIRED)
endif()

add_library(jupitr_core STATIC
    src/BellSchedule.cpp
    src/ScheduleConfig.cpp
    src/Scraper.cpp
    include/Scraper.h
    resources/jupitr.qrc
)
target_include_directories(jupitr_core PUBLIC include)
target_link_libraries(jupitr_core PUBLIC Qt6::Core Qt6::Network)
target_compile_features(jupitr_core PUBLIC cxx_std_20)

qt_add_executable(jupitr
    src/main.cpp
    src/Theme.cpp
    src/TrayApp.cpp
    src/PopupWindow.cpp
    src/SettingsWindow.cpp
    include/TrayApp.h
    include/PopupWindow.h
    include/SettingsWindow.h
)
target_link_libraries(jupitr PRIVATE jupitr_core Qt6::DBus Qt6::Gui Qt6::Widgets Qt6::Svg)
target_include_directories(jupitr PRIVATE ${XCB_INCLUDE_DIR})
target_link_libraries(jupitr PRIVATE ${XCB_LIBRARY})
target_compile_definitions(jupitr PRIVATE JUPITR_VERSION="${PROJECT_VERSION}")

include(GNUInstallDirs)
install(TARGETS jupitr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/jupitr.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ../shared/assets/jupitr.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME jupitr.svg)

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(jupitr_tests tests/test_logic.cpp)
    target_link_libraries(jupitr_tests PRIVATE jupitr_core Qt6::Test)
    target_compile_definitions(jupitr_tests PRIVATE
        JUPITR_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../shared/test-data/calendar")
    add_test(NAME jupitr_tests COMMAND jupitr_tests)
endif()
