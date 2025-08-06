# DeviceCommand.Qt

这是一个用 Qt 6 写的设备指令调度库。

项目主要用来处理设备指令的排队、发送和响应。底层通信可以是串口、TCP、BLE，
也可以是厂商自己的 SDK，核心库不直接依赖这些模块。

目前支持：

- FIFO 指令队列
- 一次执行一条指令
- sequence 和响应匹配
- 超时和重试
- 取消当前指令或清空队列
- 设备断开和发送失败处理

项目里带了一个简单的 Demo 协议和 Mock Transport，主要用于跑示例和测试，
不对应真实设备协议。

## 编译

需要准备 Qt 6、CMake 3.21 以上版本和支持 C++17 的编译器。

```bash
cmake -S . -B build \
  -DDEVICECOMMAND_BUILD_SAMPLE=ON \
  -DDEVICECOMMAND_BUILD_TESTS=ON

cmake --build build
ctest --test-dir build --output-on-failure
```

## 简单用法

```cpp
MockDeviceTransport transport;
auto codec = std::make_unique<DemoCommandCodec>();
DeviceCommandDispatcher dispatcher(&transport, std::move(codec));

DeviceCommand command;
command.commandId = 0x01;
command.payload = QByteArray::fromHex("010203");
command.timeout = std::chrono::milliseconds(2000);
command.maxRetryCount = 2;

dispatcher.enqueue(command);
```

接入真实设备时，需要实现两个接口：

- `IDeviceTransport`：负责收发数据和报告连接状态
- `IDeviceCommandCodec`：负责协议编码、拆包和响应匹配

Dispatcher 只负责指令流程，不关心底层使用哪种通信方式。

## 目录

```text
src/                    核心代码
samples/ConsoleSample/  示例程序
tests/                  单元测试
docs/                   简单的设计说明
```

## 其他

当前版本只处理单个设备和单个指令队列，没有实现自动重连、多设备管理和指令优先级。
这些功能后面有需要再增加。

## License

MIT License，详见 [LICENSE](LICENSE)。
