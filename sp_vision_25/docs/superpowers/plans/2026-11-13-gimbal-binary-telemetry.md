# 云台 CSV 控制与二进制状态回传实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 保留 STM32 接收带 CRC 的 ASCII CSV 控制帧，同时恢复上位机所需的二进制云台状态回传，使 `standard_mpc` 能够获取四元数、模式和云台状态。

**架构：** STM32 以 10 ms 周期发送 `GimbalToVision` 兼容的二进制状态帧，上位机继续以 ASCII CSV 发送控制帧。Jetson 的 udev 规则将 `/dev/gimbal` 固定到 STM32 的 `/dev/ttyACM0`，上位机从配置中的 `/dev/gimbal` 打开设备。

**技术栈：** C/C++17、STM32 USB CDC、CRC16、Eigen、CMake、udev。

---

### 任务 1：增加 STM32 二进制云台状态帧

**文件：**
- 修改：`/home/whiper/sentry_ctrl/application/robot_comm.c`
- 修改：`/home/whiper/sentry_ctrl/application/robot_comm.h`

- [x] 定义与上位机 `GimbalToVision` 字节布局一致的打包结构，包含 `SP`、模式、四元数、yaw/pitch 状态、弹速、子弹计数和 CRC16。
- [x] 使用 `get_INS_quat_point()`、云台电机反馈和当前 CSV 命令模式填充状态帧。
- [x] 以 USB CDC 每 10 ms 发送状态帧，同时保留 ASCII CSV 控制帧解析。
- [x] 保证状态帧 CRC 使用工程已有的 `append_CRC16_check_sum()`。

### 任务 2：修复 Jetson 串口别名

**文件：**
- 修改：Jetson `/etc/udev/rules.d/99-usb-serial.rules`

- [x] 将 STM32 Virtual COM 的规则从 `ttyTHS1` 改为 `ttyACM0`，并保留固定 `/dev/gimbal` 别名。
- [x] 重新加载 udev 规则，确认 `/dev/gimbal` 解析到 `/dev/ttyACM0`。

### 任务 3：编译和协议验证

**文件：**
- 使用：`/home/whiper/sentry/sp_vision_25/io/gimbal/gimbal.cpp`
- 使用：`/home/whiper/sentry/sp_vision_25/io/gimbal/gimbal.hpp`
- 使用：`/home/whiper/sentry/sp_vision_25/src/standard_mpc.cpp`

- [x] 重新编译 `standard_mpc`，确保二进制包含当前 CSV 控制协议。
- [x] 运行协议单元测试和静态检查。
- [x] STM32 重新烧录后，通过 `/dev/gimbal` 验证上位机能收到有效二进制状态帧；已确认 `SP` 帧长度为 43 字节且 CRC16 校验通过。
- [x] 通过 `/dev/gimbal` 发送带 CRLF 和 CRC16 的 `mode:0` 安全停止帧，写入返回成功。
