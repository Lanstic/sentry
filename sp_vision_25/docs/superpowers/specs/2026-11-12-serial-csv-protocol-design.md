# 云台串口 CSV 通信协议设计

## 1. 背景与范围

当前上位机通过 MicroUSB 虚拟串口向 STM32 发送 `VisionToGimbal` 二进制结构体。为了便于串口调试和人工抓包，本方案将上位机到 STM32 的控制帧改为带 CRC16 的 CSV 文本帧。

本版本只传输云台控制量，不传输目标距离、目标像素坐标或弹着点坐标。目标检测和弹道计算仍由上位机完成，STM32 只执行云台控制和发射控制。

涉及的上位机代码为 [io/gimbal/gimbal.cpp](../../../io/gimbal/gimbal.cpp) 和 [io/gimbal/gimbal.hpp](../../../io/gimbal/gimbal.hpp)。CRC 算法沿用 [tools/crc.cpp](../../../tools/crc.cpp) 中的 `get_crc16()`。

## 2. 设计目标

- 使用 ASCII 文本，便于串口助手直接查看和记录。
- 使用固定字段顺序和字段名，降低 STM32 解析和人工排查复杂度。
- 使用帧头和行结束符，支持串口丢字节后的重新同步。
- 使用 CRC16，检测传输过程中的数据损坏。
- 保持当前 `mode`、yaw 和 pitch 控制语义不变。
- 允许上位机以 100 Hz 左右的频率持续发送控制帧。

## 3. 协议帧格式

```text
SP,mode:1,yaw:0.120000,yaw_vel:0.030000,yaw_acc:0.500000,pitch:-0.080000,pitch_vel:0.010000,pitch_acc:0.200000*CRC16\r\n
```

一帧由以下部分组成：

| 部分 | 内容 | 长度 | 说明 |
| --- | --- | --- | --- |
| 帧头 | `SP` | 2 字节 | 固定为 ASCII `0x53 0x50` |
| 模式 | `mode:<value>` | 变长 | 字段名固定为 `mode`，值为十进制整数，取值为 `0`、`1` 或 `2` |
| 控制量 | 6 个键值项 | 变长 | 使用 `name:value` 表示，键名和顺序固定，字段之间使用逗号分隔 |
| 校验分隔符 | `*` | 1 字节 | 标记控制数据结束 |
| CRC | `CRC16` | 4 字节 | 4 位大写十六进制 ASCII 字符 |
| 帧尾 | `\r\n` | 2 字节 | 标记一帧结束 |

### 3.1 字段定义

| 顺序 | 字段 | 类型 | 单位 | 含义 |
| --- | --- | --- | --- | --- |
| 1 | `SP` | ASCII | 无 | 帧头和重新同步标记 |
| 2 | `mode:<value>` | 十进制整数 | 无 | `0`：不控制；`1`：控制云台但不开火；`2`：控制云台并开火 |
| 3 | `yaw:<value>` | `float32` 语义 | rad | 目标 yaw 角 |
| 4 | `yaw_vel:<value>` | `float32` 语义 | rad/s | yaw 角速度 |
| 5 | `yaw_acc:<value>` | `float32` 语义 | rad/s² | yaw 角加速度 |
| 6 | `pitch:<value>` | `float32` 语义 | rad | 目标 pitch 角 |
| 7 | `pitch_vel:<value>` | `float32` 语义 | rad/s | pitch 角速度 |
| 8 | `pitch_acc:<value>` | `float32` 语义 | rad/s² | pitch 角加速度 |

键名区分大小写，必须完全匹配上表。浮点值建议由上位机使用 `%.6f` 输出。STM32 解析时允许数值长度变化，但必须拒绝未知键、重复键、空值、非数字、无穷大和超出安全范围的数值。

### 3.2 示例

控制云台但不开火，控制量为 yaw `0.12 rad`、yaw 速度 `0.03 rad/s`、yaw 加速度 `0.5 rad/s²`，pitch `-0.08 rad`、pitch 速度 `0.01 rad/s`、pitch 加速度 `0.2 rad/s²`：

```text
SP,mode:1,yaw:0.120000,yaw_vel:0.030000,yaw_acc:0.500000,pitch:-0.080000,pitch_vel:0.010000,pitch_acc:0.200000*338C\r\n
```

CRC 输入文本为：

```text
SP,mode:1,yaw:0.120000,yaw_vel:0.030000,yaw_acc:0.500000,pitch:-0.080000,pitch_vel:0.010000,pitch_acc:0.200000
```

该文本使用本项目 CRC16 算法计算得到 `338C`。

## 4. CRC16 规则

CRC 的输入范围是从第一个 `S` 开始，到 `*` 前一个数字结束的所有 ASCII 字节：

```text
SP,mode:1,yaw:0.120000,yaw_vel:0.030000,yaw_acc:0.500000,pitch:-0.080000,pitch_vel:0.010000,pitch_acc:0.200000
```

CRC 不包含以下内容：

- `*`；
- 4 个 CRC 字符；
- `\r` 和 `\n`。

算法参数与 [tools/crc.cpp](../../../tools/crc.cpp) 中的 `get_crc16()` 一致：

| 参数 | 值 |
| --- | --- |
| 初值 | `0xFFFF` |
| 反射多项式 | `0x8408` |
| 最终异或 | 无 |
| 输出格式 | 4 位大写十六进制 ASCII，例如 `E6DE` |

CRC 以文本形式传输，不使用当前二进制协议中的小端 `uint16_t` 内存布局，因此不依赖 STM32 和上位机的字节序。

## 5. STM32 接收与拆包

STM32 应使用 UART DMA 加环形缓冲区，或在接收中断中逐字节放入环形缓冲区。解析器从缓冲区取出字节，按 `\n` 查找完整帧，不应把一次 UART 回调当成一整帧。

### 5.1 拆包步骤

1. 等待字符 `S`。
2. 下一字符必须是 `P`，否则丢弃当前候选帧并重新寻找 `S`。
3. 继续接收，直到遇到 `\n`。
4. 检查前一字符是否为 `\r`，然后去除 `\r\n`。
5. 检查帧长度不超过实现定义的缓冲区上限，例如 128 字节。
6. 查找 `*`，并确认其后正好有 4 个十六进制字符。
7. 对 `*` 前的文本计算 CRC16。
8. 将 4 个 CRC 字符转换为 `uint16_t`，与计算结果比较。
9. 按逗号拆分字段，必须得到 8 个字段：`SP` 加 7 个键值项。
10. 第 2～8 个字段按第 1 个冒号拆分为键和值。
11. 按固定顺序检查键名：`mode`、`yaw`、`yaw_vel`、`yaw_acc`、`pitch`、`pitch_vel`、`pitch_acc`。
12. 解析 `mode` 和 6 个浮点值。
13. 检查模式范围、数值有限性和控制量安全范围。
14. 只有全部检查通过后，才将临时结果一次性写入当前控制目标。

### 5.2 解析器接口示例

```c
typedef struct {
    uint8_t mode;
    float yaw;
    float yaw_vel;
    float yaw_acc;
    float pitch;
    float pitch_vel;
    float pitch_acc;
} GimbalCommand;

bool parse_gimbal_frame(char *line, GimbalCommand *out);
```

逐字节接收逻辑可以采用以下结构：

```c
void uart_rx_byte(uint8_t ch)
{
    if (line_length >= LINE_BUFFER_SIZE - 1U) {
        line_length = 0;
        return;
    }

    line[line_length++] = (char)ch;

    if (ch == '\n') {
        line[line_length] = '\0';

        GimbalCommand pending;
        if (parse_gimbal_frame(line, &pending)) {
            current_command = pending;
            last_valid_tick = HAL_GetTick();
        }

        line_length = 0;
    }
}
```

`parse_gimbal_frame()` 建议按“帧头 → 帧尾 → CRC → 字段数量 → 键名和顺序 → 数值格式 → 安全范围”的顺序检查。不要在校验完成前直接更新正在使用的控制变量。

### 5.3 异常处理与安全策略

- 帧头错误、字段数错误或 CRC 错误：丢弃整帧，等待下一帧。
- 缓冲区溢出：清空当前帧，重新寻找 `S`。
- `mode` 不在 `0～2`：丢弃整帧。
- 浮点数为 `NaN`、无穷大或超出配置范围：丢弃整帧。
- 建议超过 100 ms 没有收到有效帧时进入安全状态，相当于 `mode = 0`，停止云台控制和发射。

## 6. 串口参数与发送周期

修改前的 [io/gimbal/gimbal.cpp](../../../io/gimbal/gimbal.cpp) 只设置串口路径，串口库默认参数为 9600 baud、8N1、无流控。CSV 帧比二进制帧更长，而现有规划线程约每 10 ms 发送一次，因此本次上位机实现和 STM32 固件应统一使用：

```text
波特率：115200
数据位：8
校验位：无
停止位：1
流控：无
```

上位机每次发送一整行，包括 `\r\n`，但不发送 C 字符串结束符 `\0`。

## 7. 兼容性与后续扩展

本协议仅替换上位机到 STM32 的控制帧。STM32 到上位机的状态帧暂不修改，目标距离、目标像素坐标和弹着点坐标也不加入本版本。

如果后续需要传输目标信息，应增加协议版本字段或定义新的帧头，避免 STM32 将新旧帧混在一起解析。例如可以另定义 `ST` 帧传输目标信息，而不是改变当前 8 字段控制帧的字段顺序。
