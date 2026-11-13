# 云台 CSV 串口发送实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将上位机发送给 STM32 的云台控制帧改为带字段名和 CRC16 的 CSV 文本帧，并设置适合高频发送的串口参数。

**架构：** 将 CSV 序列化逻辑从 `Gimbal` 串口生命周期中分离为独立的协议模块。`Gimbal::send()` 只负责构造控制数据、调用序列化函数并写入串口；STM32 到上位机的状态帧解析保持现状。

**技术栈：** C++17、现有 `serial` 库、项目 `tools::get_crc16()`、CMake/CTest。

---

## 文件结构

- 创建：`io/gimbal/protocol.hpp`，定义上位机控制数据结构和 CSV 序列化接口。
- 创建：`io/gimbal/protocol.cpp`，实现字段名格式、固定 6 位小数、CRC16 文本和 `\r\n` 帧尾。
- 修改：`io/gimbal/gimbal.hpp`，使用协议结构，移除二进制发送缓冲区成员。
- 修改：`io/gimbal/gimbal.cpp`，设置 115200 baud，并通过协议序列化函数发送文本帧。
- 修改：`io/CMakeLists.txt`，把协议实现加入 `io` 静态库。
- 修改：`CMakeLists.txt`，添加协议单元测试目标并注册 CTest。
- 创建：`tests/gimbal_protocol_test.cpp`，验证示例帧、字段顺序、固定小数和 CRC。

## 任务 1：编写协议序列化失败测试

**文件：**
- 创建：`tests/gimbal_protocol_test.cpp`
- 修改：`CMakeLists.txt`

- [x] **步骤 1：添加测试目标和断言**

测试构造一个控制命令，并断言序列化结果完全等于协议文档中的帧：

```cpp
io::VisionToGimbal command{
  1, 0.12F, 0.03F, 0.5F, -0.08F, 0.01F, 0.2F};
assert(
  io::serialize_vision_to_gimbal(command) ==
  "SP,mode:1,yaw:0.120000,yaw_vel:0.030000,yaw_acc:0.500000,"
  "pitch:-0.080000,pitch_vel:0.010000,pitch_acc:0.200000*338C\r\n");
```

测试还应覆盖 `mode = 0`，确认模式值不会被格式化成浮点数，并检查所有字段名和字段顺序只出现一次。

- [x] **步骤 2：运行测试确认失败**

运行：

```bash
g++ -std=c++17 -I. -Iio/serial/include -c tests/gimbal_protocol_test.cpp -o build/gimbal_protocol_test.o
```

实际结果：由于 `io/gimbal/protocol.hpp` 尚未实现，编译按预期失败。

## 任务 2：实现 CSV 序列化模块

**文件：**
- 创建：`io/gimbal/protocol.hpp`
- 创建：`io/gimbal/protocol.cpp`
- 修改：`io/CMakeLists.txt`

- [x] **步骤 1：定义协议接口**

```cpp
struct VisionToGimbal
{
  uint8_t mode;
  float yaw;
  float yaw_vel;
  float yaw_acc;
  float pitch;
  float pitch_vel;
  float pitch_acc;
};

std::string serialize_vision_to_gimbal(const VisionToGimbal & command);
```

- [x] **步骤 2：实现最少序列化逻辑**

使用经典 C++ locale、固定 6 位小数和以下字段顺序构造 CRC 输入：

```text
SP,mode:<mode>,yaw:<yaw>,yaw_vel:<yaw_vel>,yaw_acc:<yaw_acc>,pitch:<pitch>,pitch_vel:<pitch_vel>,pitch_acc:<pitch_acc>
```

调用 `tools::get_crc16()`，将 CRC 写成 4 位大写十六进制文本，最后追加 `*CRC\r\n`。

- [x] **步骤 3：运行测试确认通过**

运行：

```bash
g++ -std=c++17 -I. tests/gimbal_protocol_test.cpp io/gimbal/protocol.cpp tools/crc.cpp -o build/gimbal_protocol_test.manual
build/gimbal_protocol_test.manual
```

实际结果：隔离测试编译成功并以退出码 0 运行完成。

## 任务 3：切换 Gimbal 串口发送

**文件：**
- 修改：`io/gimbal/gimbal.hpp`
- 修改：`io/gimbal/gimbal.cpp`

- [x] **步骤 1：替换发送缓冲区类型**

让 `gimbal.hpp` 包含协议头文件，移除 `VisionToGimbal` 中的二进制 `crc16` 字段以及 `Gimbal::tx_data_` 成员；接收方向的 `GimbalToVision` 保持不变。

- [x] **步骤 2：设置串口参数**

在 `setPort()` 和 `open()` 之间调用：

```cpp
serial_.setBaudrate(115200);
```

其他串口参数保持 8N1、无流控。

- [x] **步骤 3：改写两个 send 重载**

`send(bool, bool, ...)` 构造 `VisionToGimbal` 后调用另一个重载；另一个重载调用 `serialize_vision_to_gimbal()`，将返回的完整字符串传给 `serial_.write()`。接收线程仍按原二进制 `GimbalToVision` 帧工作。

- [x] **步骤 4：运行相关编译验证**

使用 `g++` 编译 `io/gimbal/gimbal.cpp` 对象和 `tests/fire_test.cpp` 对象，确认协议模块和现有 `Gimbal` API 均能通过编译。当前 VS Code CMake Tools 未识别此嵌套项目，完整 CMake 构建留待 Jetson 工程环境验证。

## 任务 4：Jetson 迁移核对

**文件：**
- 参考：`docs/superpowers/specs/2026-11-12-serial-csv-protocol-design.md`

- [x] **步骤 1：记录迁移文件**

迁移至少包含协议头源文件、`gimbal.hpp`、`gimbal.cpp`、两个 CMake 文件以及协议文档。

- [x] **步骤 2：记录构建与串口检查命令**

在 Jetson 上重新配置并构建 `gimbal_protocol_test` 和主程序，确认 `/dev/gimbal` 指向 STM32 虚拟串口，并确认 UART 两端均为 115200 8N1。

## 任务 5：提交

- [ ] **步骤 1：检查差异和工作区**

运行：

```bash
git diff --check
git status --short
```

- [ ] **步骤 2：提交实现**

```bash
git add CMakeLists.txt io/CMakeLists.txt io/gimbal/gimbal.hpp io/gimbal/gimbal.cpp io/gimbal/protocol.hpp io/gimbal/protocol.cpp tests/gimbal_protocol_test.cpp docs/superpowers/plans/2026-11-12-gimbal-csv-implementation.md
git commit -m "feat(云台通信): 发送带字段名的 CSV 控制帧"
```
