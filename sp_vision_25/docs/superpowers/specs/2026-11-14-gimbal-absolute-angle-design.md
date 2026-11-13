# 云台绝对角度控制修复设计

## 目标

让 STM32 下位机把视觉端发送的 yaw/pitch 解释为陀螺仪绝对角度目标，避免把世界坐标绝对角度误当成编码器相对角度，导致目标位于正前方时云台突然转动。

## 当前根因

上位机 `Planner::aim()` 根据目标世界坐标计算绝对 yaw 和弹道 pitch。下位机 USB 控制分支却选择 `GIMBAL_MOTOR_ENCONDE`，并将这两个值写入 `relative_angle_set`。因此上位机和下位机使用了不同的角度参考系。

## 设计

### 下位机控制

- USB 有效命令时，yaw 和 pitch 电机使用 `GIMBAL_MOTOR_GYRO`。
- 每个控制周期先以当前绝对角度为基准，再将命令角度与当前绝对角度的差值交给现有绝对角度限位函数。
- yaw 使用现有的角度环绕逻辑；pitch 复用现有相对机械限位检查。
- 不使用 `yaw_vel`、`pitch_vel`、`yaw_acc`、`pitch_acc` 改变控制器行为，保持当前协议兼容性。

### 状态回传

- 二进制状态帧中的 yaw/pitch 改为回传 `absolute_angle`。
- 四元数、帧布局、CRC 和通信频率保持不变。

### 上位机

- `planner.cpp` 不修改。它已经输出世界坐标绝对 yaw/pitch。
- 上位机协议字段和单位保持不变。

## 限位和模式切换

- 切换到陀螺仪模式时，现有模式切换函数先把目标设为当前绝对角度，随后 USB 命令更新目标，避免切换瞬间产生阶跃。
- pitch 的目标角度通过现有绝对角度限位逻辑约束到机械范围。
- yaw 继续使用 `rad_format()` 处理跨越 `-pi/pi` 的角度差。

## 实现记录

本方案已经完成下位机代码修改，固件仓库提交为 `1bd48bd`（`fix(云台): 使用陀螺仪绝对角度控制`）：

- [gimbal_task.c](/home/whiper/sentry_ctrl/application/gimbal_task.c) 的 USB 有效命令分支改用 `GIMBAL_MOTOR_GYRO`。每个控制周期先以当前 `absolute_angle` 初始化目标，再将视觉端命令与当前绝对角度的差值交给现有 `gimbal_absolute_angle_limit()`，不再把视觉端绝对角度写入 `relative_angle_set`。
- [robot_comm.c](/home/whiper/sentry_ctrl/application/robot_comm.c) 的云台状态回传改为发送 yaw/pitch 的 `absolute_angle`，保持帧布局、单位、CRC 和通信频率不变。
- [README_ASCII_PROTOCOL.md](/home/whiper/sentry_ctrl/README_ASCII_PROTOCOL.md) 已同步说明状态帧中的 yaw/pitch 是绝对角度。
- [planner.cpp](/home/whiper/sentry/sp_vision_25/tasks/auto_aim/planner/planner.cpp) 未修改；上位机继续发送世界坐标系下的绝对 yaw/pitch，配置文件也无需增加新的字段。

已完成的验证：

- 下位机源码静态检查通过，USB 控制分支不再写入 `relative_angle_set`。
- 上位机现有 `gimbal_protocol_test.manual` 运行通过，通信格式相关检查未发现回归。

尚未完成的验证：当前环境没有 Keil/ArmCC 工具链，无法构建或烧录 STM32 固件；因此还需要在目标开发环境完成固件编译，并在架空、卸弹条件下验证正前方目标不再触发 yaw 猛转，以及回传绝对角度与上位机规划角度的一致性。

## 验证

1. 静态检查下位机控制分支不再写入 `relative_angle_set`。
2. 使用 Keil 工程配置构建 `standard_tpye_c`；若当前环境没有 Keil 工具链，记录未完成固件构建。
3. 构建上位机现有协议测试，确认通信格式没有改变。
4. 实机架空、卸弹后验证：目标在正前方时 yaw 不再回到编码器零点；目标左右移动时绝对角度误差能够收敛。
5. PlotJuggler 对比回传的绝对 yaw/pitch 与上位机 `plan_yaw/plan_pitch`。

## 风险

- `AHRS.lib` 是预编译库，四元数内部顺序无法仅由源码完全证明；本次不改变四元数顺序。
- 如果 IMU 安装矩阵或 `R_gimbal2imubody` 不正确，绝对角度闭环仍会有方向或零点偏差，需要单独校准。
- 实机测试必须限制云台运动范围并准备立即断电。
