# 车载多线程传感器数据采集框架

> 基于 ARM Linux 的车载行车数据记录仪上位机 · I.MX6U-ALPHA / Linux 4.1.15
> 下位机 STM32F103 经 CAN 总线上报四类传感器数据，本框架负责接收、解析、告警检测、可靠存储与 TCP 实时推送。

[![Platform](https://img.shields.io/badge/platform-ARM%20Linux-orange)](#)
[![Language](https://img.shields.io/badge/language-C-blue)](#)
[![Toolchain](https://img.shields.io/badge/toolchain-Linaro%204.9.4-green)](#)
[![Stability](https://img.shields.io/badge/uptime-9h12m-brightgreen)](#)

## 概览

这是毕业设计《基于嵌入式Linux的车载行车数据记录仪》的**上位机核心工程**。它复刻了真实车载电子的分布式架构——下位机 MCU 作为传感器 ECU 负责实时采集，本系统作为中央网关节点负责数据汇聚、持久化与人机交互。

系统已在硬件上完成 **9 小时 12 分连续稳定性测试**：33,120 条数据完整率 **99.97%**，CAN 接收 113 万包零错误，内存平台化后 4 小时零增长（无泄漏）。

## 特性

- **SocketCAN 通信**：`socket(PF_CAN, SOCK_RAW, CAN_RAW)` 四步曲 + `SO_RCVTIMEO` 1s；ENETDOWN 限速重试、链路恢复自动续收
- **多线程流水线**：七线程（gps / sensor / can / collector / process / store / tcp），两级生产者-消费者环形缓冲，反压与 1:1 零丢失
- **自定义 CAN 应用层协议**：数据帧 0x100~0x10A + 事件帧 0x300/0x301/0x302；float 小端 `memcpy`、int16 大端移位解析；加速度 0.01g 单位换算
- **可靠存储**：SQLite WAL 模式 + NORMAL 同步，双表设计（`data_point` 状态表 12 列 + `alert_log` 事件表），事务批量写、预编译插入
- **优雅退出**：SIGINT → `g_running=0`；消费者 `rb_pop_timeout`、CAN `SO_RCVTIMEO`、TCP `poll` 超时；`pthread_join` 收齐全部线程
- **TCP 实时通道**：橱窗模式推送最新数据，事件经独立通道秒级上屏（告警延迟从 35s 优化到 2s）

## 系统架构

```
STM32F103（传感器 ECU）              I.MX6ULL ALPHA（网关节点）
GPS / DHT11 / MPU6050 / HC-SR04           SocketCAN can0 (500 kbps)
            │  bxCAN / TJA1050                    │
            └────────── CAN 总线 ─────────────────┤
                                                   ▼ can_thread（CanData + 互斥锁）
                                              collector（1 Hz 组装 DataPoint）
                                                   ▼ ①号环形 buffer
                                              process（告警检测）
                                                   ▼ ②号环形 buffer
                                          ┌────────┴────────┐
                                          ▼                 ▼
                                     store（SQLite）    tcp（实时推送）
                                          ▼                 ▼
                                   vehicle.db          Qt 监控端 (:9000)
                                   + db_query
```

## 数据表

`data_point`（12 列）：`timestamp, latitude, longitude, speed, accel_total, temperature, humidity, pitch, roll, distance, satellites, alert`

`alert_log`：`timestamp, alert_type, detail`

两张表均 append-only 只插入不修改，符合 EDR（事故数据记录仪）形态的历史对账要求。

## 快速开始

**编译环境**：Linaro 4.9.4 `arm-linux-gnueabihf-`（需前置到 PATH）；`-std=gnu99`；`SQLITE_DIR` 指向交叉编译的 SQLite3。

```bash
# 编译
make                          # 产物: sensor_framework (ARM EABI5)
                              # db_query 独立编译, 不进 OBJS

# 板子上运行
date -s "YYYY-MM-DD HH:MM:SS"        # 先对时（RTC 视后备电池而定）
ip -details link show can0           # S99can 应已自动 UP / 500k
cd /home/root
stdbuf -oL ./sensor_framework > /home/root/run.log 2>&1 &
disown

# 查询
./db_query stats
./db_query range "start-time" "end-time"
./db_query alerts
```

## 实测数据（9h12m 长跑，2026-10-31）

| 指标 | 结果 |
|---|---|
| 连续运行 | 9 小时 12 分 09 秒 |
| 数据完整率 | 99.97%（33,120 条 / 33,129 s） |
| 内存 | 1,884 → 4,120 kB 后平台化，其后 4 小时零增长（SQLite 页缓存，非泄漏） |
| CAN | RX 1,138,600 包，errors / dropped / overrun 全 0 |
| 查询 | `COUNT(*)` 0.028 s（4.7 万行） |
| 存储 | 139 B/条，6.5 MB / 46,920 行 |
| 崩溃 | 0 次 |
| 端到端延迟 | TCP 传输 ≈1 s（对比文件同步通道 30~40 s，约 10×） |

## 已知边界

- 室内 GPS 未定位时 0x100/101 停发；开机自检测试坐标（sat=8）已修复，引用历史数据需注明采集日期
- RTC 依赖后备电池，每次开测先 `date` 确认
- `satellites=8` 的历史行属自检残留期采集，论文引用需注明

## 目录结构

```
├── src/            # 核心源码（can_linux / can_parser / thread_* / database / ringbuf ...）
├── include/        # 头文件
├── tools/          # start_system.sh（一键启动）、verify_full_chain.py（自动化对数）
├── docs/arch.png   # 架构图
├── test_ringbuf.c  # 环形缓冲单测
└── Makefile
```

## 相关仓库

- 下位机与全链路文档：本仓库 `docs/`
- 界面层：[09_qt_interface](https://github.com/WQZ107/09_qt_interface)
- 联网上报：[10_net_gateway](https://github.com/WQZ107/10_net_gateway)
