# 发布前检查

- 用支持的 Qt 6 版本配置项目，确认 CMake 能正常生成构建目录。
- 用 Release 模式编译核心库、Console Sample 和 Qt Test。
- 执行 `ctest --test-dir build --output-on-failure`，不要只看编译是否成功。
- 运行 Console Sample，确认它能收到 Mock 响应并正常退出。
- 检查核心 target 没有混入串口、TCP、BLE、USB、厂商 SDK 或真实设备协议。
- 确认超时、重试、取消、断开、半包、粘包和校验错误测试都通过。
- 看一遍公共头文件的注释，确认没有把内部可修改对象暴露出去。
- 检查 README 中的构建命令、版本号和目录说明是否仍然准确。
- 更新 `CHANGELOG.md` 和 CMake 中的项目版本。
- 确认 MIT License 中的版权信息正确。
- GitHub Actions 通过后再打 release tag。
