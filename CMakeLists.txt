cmake_minimum_required(VERSION 3.21)

project(DeviceCommandQt
    VERSION 0.1.0
    DESCRIPTION "A lightweight Qt device command dispatcher"
    LANGUAGES CXX
)

option(DEVICECOMMAND_BUILD_SAMPLE "Build the console sample" ON)
option(DEVICECOMMAND_BUILD_TESTS "Build Qt Test unit tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(DeviceCommandQt STATIC
    src/DeviceCommandTypes.h
    src/IDeviceTransport.h
    src/IDeviceCommandCodec.h
    src/DeviceCommandDispatcher.h
    src/DeviceCommandDispatcher.cpp
)

add_library(DeviceCommandQt::DeviceCommandQt ALIAS DeviceCommandQt)

target_compile_features(DeviceCommandQt PUBLIC cxx_std_17)

target_include_directories(DeviceCommandQt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/DeviceCommandQt>
)

target_link_libraries(DeviceCommandQt PUBLIC Qt6::Core)

set_target_properties(DeviceCommandQt PROPERTIES
    OUTPUT_NAME device-command-qt
    POSITION_INDEPENDENT_CODE ON
)

if(DEVICECOMMAND_BUILD_SAMPLE)
    add_subdirectory(samples/ConsoleSample)
endif()

if(DEVICECOMMAND_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
