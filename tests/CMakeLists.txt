find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(DeviceCommandTests
    DeviceCommandTests.cpp
    ../samples/ConsoleSample/DemoCommandCodec.h
    ../samples/ConsoleSample/DemoCommandCodec.cpp
)

target_include_directories(DeviceCommandTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../samples/ConsoleSample
)

target_link_libraries(DeviceCommandTests
    PRIVATE
        DeviceCommandQt
        Qt6::Core
        Qt6::Test
)

add_test(NAME DeviceCommandTests COMMAND DeviceCommandTests)
