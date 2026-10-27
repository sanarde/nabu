cmake_minimum_required(VERSION 3.21)
project(nabu-popup VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick DBus)
find_package(SanarStyle REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

include(GNUInstallDirs)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
