# DeviceCommand.Qt

DeviceCommand.Qt 是一个基于 C++17、Qt 6 Core 和 CMake 的轻量级设备指令库。
项目希望把设备指令的数据结构、传输接口和协议编解码职责拆分开，避免核心逻辑
直接依赖串口、TCP、BLE 或厂商 SDK。

## 初始范围

- 定义通用的指令、响应和执行结果；
- 抽象设备传输和协议编解码接口；
- 逐步实现串行指令调度、响应匹配、超时和重试；
- 提供示例和自动化测试。

## 核心接口

`IDeviceTransport` 只负责收发字节并报告连接状态，`IDeviceCommandCodec` 负责把
指令编码成帧、从字节流拆出响应并判断响应是否匹配。具体的串口、网络或厂商 SDK
由接入方实现，不会耦合进核心库。

## 调度器

`DeviceCommandDispatcher` 负责 FIFO 排队，一次只发送一条指令，并在收到匹配响应后
继续处理下一条。指令会保留自己的 sequence，后续版本将补充超时、重试、取消和
断开处理。

## 示例

`samples/ConsoleSample` 提供虚构帧协议、内存 Transport 和控制台程序，用于演示
指令入队、发送和响应匹配，不对应任何真实设备协议。

## 构建要求

- C++17 编译器；
- Qt 6 Core；
- CMake 3.21 或更高版本。

```bash
cmake -S . -B build
cmake --build build
```

## License

MIT License，详见 [LICENSE](LICENSE)。
