# DeviceCommand.Qt

DeviceCommand.Qt 是一个基于 C++17、Qt 6 Core 和 CMake 的轻量级设备指令库。
项目希望把设备指令的数据结构、传输接口和协议编解码职责拆分开，避免核心逻辑
直接依赖串口、TCP、BLE 或厂商 SDK。

## 初始范围

- 定义通用的指令、响应和执行结果；
- 抽象设备传输和协议编解码接口；
- 逐步实现串行指令调度、响应匹配、超时和重试；
- 提供示例和自动化测试。

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
