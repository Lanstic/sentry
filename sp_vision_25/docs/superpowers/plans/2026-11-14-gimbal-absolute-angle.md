# 云台绝对角度控制修复实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 让 STM32 将视觉端的世界绝对 yaw/pitch 目标交给陀螺仪绝对角度闭环，避免把绝对角度误写入编码器相对角度目标。

**架构：** 上位机 `Planner::aim()` 和 CSV 协议保持不变。下位机 USB 有效命令改用 `GIMBAL_MOTOR_GYRO`，以当前绝对角度为基准调用现有绝对角度限位逻辑，再写入 `absolute_angle_set`。状态帧回传 yaw/pitch 改为绝对角度，保证调试曲线与控制命令使用同一参考系。

**技术栈：** STM32F407 C、FreeRTOS、Keil 工程、C++17 上位机、CMake/CTest。

---

## 文件清单

- 修改 `/home/whiper/sentry_ctrl/application/gimbal_task.c`：USB 云台命令的控制模式和绝对目标写入。
- 修改 `/home/whiper/sentry_ctrl/application/robot_comm.c`：状态帧 yaw/pitch 回传字段改为绝对角度。
- 修改 `/home/whiper/sentry/sp_vision_25/docs/superpowers/specs/2026-11-14-gimbal-absolute-angle-design.md`：设计已完成并提交，本计划不改变设计范围。
- 不修改 `/home/whiper/sentry/sp_vision_25/tasks/auto_aim/planner/planner.cpp`：确认其继续输出世界绝对角度。

### 任务 1：建立上位机输出语义检查

**文件：** `/home/whiper/sentry/sp_vision_25/tasks/auto_aim/planner/planner.cpp`

- [x] 检查 `Planner::aim()` 保持 `atan2(x,y)` 世界 yaw 和弹道 pitch 的现有输出。
- [x] 运行现有上位机协议测试，确认无需修改发送字段、单位和顺序。

运行：

```bash
cmake --build /home/whiper/sentry/sp_vision_25/build --target gimbal_protocol_test
ctest --test-dir /home/whiper/sentry/sp_vision_25/build -R gimbal_protocol_test --output-on-failure
```

预期：协议测试通过，且 `planner.cpp` 无工作区改动。

### 任务 2：切换下位机 USB 控制到绝对角度模式

**文件：** `/home/whiper/sentry_ctrl/application/gimbal_task.c`

- [x] 在 USB 命令有效分支将 yaw/pitch 电机模式设为 `GIMBAL_MOTOR_GYRO`。
- [x] 进入绝对模式后，以当前 `absolute_angle` 初始化目标，保留现有模式切换保护。
- [x] 每个控制周期用 `rad_format(command - absolute_angle)` 得到目标差值，调用 `gimbal_absolute_angle_limit()` 更新 `absolute_angle_set`。
- [x] 删除该分支对 `relative_angle_set` 的直接赋值；不要使用编码器相对角度 PID 执行视觉命令。
- [x] 保持 `yaw_vel`、`pitch_vel`、`yaw_acc`、`pitch_acc` 的现有保存行为，不在本次改动中引入新的控制环。

目标代码形态：

```c
gimbal_control.gimbal_yaw_motor.gimbal_motor_mode = GIMBAL_MOTOR_GYRO;
gimbal_control.gimbal_pitch_motor.gimbal_motor_mode = GIMBAL_MOTOR_GYRO;
...
gimbal_control.gimbal_yaw_motor.absolute_angle_set =
    gimbal_control.gimbal_yaw_motor.absolute_angle;
gimbal_absolute_angle_limit(
    &gimbal_control.gimbal_yaw_motor,
    rad_format(gimbal_cmd.yaw - gimbal_control.gimbal_yaw_motor.absolute_angle));
```

pitch 使用相同流程，因此复用现有 pitch 机械限位。yaw 使用现有 `rad_format()` 环绕处理。

### 任务 3：统一状态回传参考系

**文件：** `/home/whiper/sentry_ctrl/application/robot_comm.c`

- [x] 将 `send_gimbal_state()` 的 `yaw` 从 `yaw->relative_angle` 改为 `yaw->absolute_angle`。
- [x] 将 `pitch` 从 `pitch->relative_angle` 改为 `pitch->absolute_angle`。
- [x] 保持四元数 `memcpy`、43 字节 packed 布局、CRC16、10 ms 周期和速度字段不变。
- [x] 同步更新 `/home/whiper/sentry_ctrl/README_ASCII_PROTOCOL.md` 对状态帧 yaw/pitch 参考系的说明。

### 任务 4：静态检查和固件构建

**文件：** `/home/whiper/sentry_ctrl/application/gimbal_task.c`、`/home/whiper/sentry_ctrl/application/robot_comm.c`

- [x] 检查 USB 有效命令分支只写 `absolute_angle_set`，且控制循环进入 `gimbal_motor_absolute_angle_control()`。
- [ ] 使用 Keil 工程 `MDK-ARM/standard_tpye_c.uvprojx` 构建 `standard_tpye_c`。
- [x] 确认当前环境没有 Keil/ArmCC；未使用旧 `.o` 或旧日志冒充本次构建结果。

### 任务 5：上位机回归验证

**文件：** `/home/whiper/sentry/sp_vision_25/tasks/auto_aim/planner/planner.cpp`、`/home/whiper/sentry/sp_vision_25/io/gimbal/protocol.cpp`

- [ ] 构建 `standard_mpc` 和现有通信测试。
- [x] 确认 CSV 中 yaw/pitch 仍是 rad，发送格式和 CRC 不变。
- [ ] 使用 PlotJuggler 对比回传绝对 yaw/pitch 与 `plan_yaw/plan_pitch`。

### 任务 6：安全实机验证

- [ ] 架空底盘、卸弹、限制云台运动范围并准备立即断电。
- [ ] 在无目标时发送 `mode:0`，确认云台不接收新的绝对目标。
- [ ] 目标位于正前方时确认 yaw 不再回到编码器零点。
- [ ] 缓慢移动目标左右两侧，确认绝对角度误差向零收敛且没有持续加速。
- [ ] 若出现持续顶限位、反向加速或大幅旋转，立即断电并回退固件。
