add_executable(DeviceCommandConsoleSample
    main.cpp
    DemoCommandCodec.h
    DemoCommandCodec.cpp
    MockDeviceTransport.h
    MockDeviceTransport.cpp
)

target_link_libraries(DeviceCommandConsoleSample
    PRIVATE
        DeviceCommandQt
        Qt6::Core
)

set_target_properties(DeviceCommandConsoleSample PROPERTIES
    OUTPUT_NAME device-command-sample
)
