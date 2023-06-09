# 指令执行流程

Dispatcher 只保留一个当前指令，后面的指令按进入顺序放在 FIFO 队列里。当前指令结束
后，才会取出下一条。

```text
Idle
  -> Sending
  -> WaitingResponse
  -> Completed / Failed / Cancelled
  -> Idle 或下一条 Sending
```

超时后，如果还有重试次数，就进入 `Retrying`，等待固定间隔后重新发送。重试始终使用
同一个 sequence。响应成功、失败或取消都会先停掉计时器，并且只发出一次终态信号。

Demo 协议使用虚构的 `AA 55` 请求帧和 `55 AA` 响应帧，字段按大端序排列，校验使用 XOR。
它只是为了让示例和测试跑起来，不对应任何真实设备协议。
