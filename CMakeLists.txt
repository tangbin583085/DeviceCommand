cmake_minimum_required(VERSION 3.21)

project(DeviceCommandQt
    VERSION 0.1.0
    DESCRIPTION "A lightweight Qt device command dispatcher"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(DeviceCommandQt INTERFACE)

add_library(DeviceCommandQt::DeviceCommandQt ALIAS DeviceCommandQt)

target_compile_features(DeviceCommandQt INTERFACE cxx_std_17)

target_include_directories(DeviceCommandQt
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/DeviceCommandQt>
)

target_link_libraries(DeviceCommandQt INTERFACE Qt6::Core)
