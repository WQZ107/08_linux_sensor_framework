# 车载多线程传感器数据采集框架（I.MX6U-ALPHA / Linux）

> 毕设《基于嵌入式Linux的车载行车数据记录仪》上位机软件骨架
> **版本：v1.0（2026-10-23，126-140 CAN 全链路验收完成）**

## 系统架构

```
STM32F103（传感器节点）                    I.MX6ULL ALPHA（汇聚节点）
GPS/DHT11/MPU6050/HC-SR04 → bxCAN/TJA1050 → SocketCAN can0
                                               ↓ can_thread（CanData + 互斥锁）
                                          collector（1Hz 组装 DataPoint）
                                               ↓ ①号环形 buffer
                                          process（告警检测）
                                               ↓ ②号环形 buffer
                                          store（SQLite 事务批量）
                                               ↓
                                          vehicle.db + db_query
```

- **六线程**：gps / sensor / can / collector / process / store
- **两级生产者-消费者流水线**：反压、1:1 零丢失
- **优雅退出**：SIGINT → `g_running=0`；消费者 `rb_pop_timeout`；CAN `SO_RCVTIMEO`；`pthread_join` 收齐

## 关键实现

- **SocketCAN**：`socket(PF_CAN,SOCK_RAW,CAN_RAW)` → `ioctl(SIOCGIFINDEX)` → `bind` → `read/write`；`SO_RCVTIMEO` 1s；ENETDOWN 限速重试、恢复自动续收
- **协议解析**：float 小端 `memcpy`、int16 大端移位；数据帧 0x100~0x10A + 事件帧 0x300/0x301/0x302；加速度 **0.01g ÷100**
- **SQLite**：WAL + NORMAL；`data_point`（12 列）+ `alert_log`；每 10 条事务批量；预编译插入
- **开机脚本**：`/etc/rc5.d/S99can`（can0 500k 自动 UP）

## 数据表（data_point）

12 列：`timestamp, latitude, longitude, speed, accel_total, temperature, humidity, pitch, roll, distance, satellites, alert`
（`alert_log`：`timestamp, alert_type, detail`）

## 编译

- 工具链：Linaro 4.9.4 `arm-linux-gnueabihf-`（PATH 前置）
- `make`（`-std=gnu99`；`SQLITE_DIR` 指向交叉编译的 sqlite3）
- 产物：`sensor_framework`（ARM EABI5）；`db_query` 独立编译（不进 OBJS）

## 运行

```bash
date -s "YYYY-MM-DD HH:MM:SS"        # 先 date 确认；板子 RTC 视后备电池而定
ip -details link show can0           # S99can 应已自动 UP/500k
cd /home/root
stdbuf -oL ./sensor_framework > /home/root/run.log 2>&1 &
disown

./db_query stats
./db_query range "start-time" "end-time"
./db_query alerts
```

## 验收数据（2026-10-23，4h 长跑）

- 时长 **4h02m45s**（14,565 s），新行 **14,561**，完整率 **99.97%**，**零丢失**（存储=COUNT）
- CAN RX **559,950 帧**，errors/dropped/overrun 全 **0**（≈38.4 帧/s）
- DB **2.1 MB**，≈**151 B/条**；`COUNT(*)` **0.021 s**
- CPU **0.5%**；VmRSS 1904→4120 kB（**SQLite 页缓存 ~2MB，非泄漏**）
- 异常注入：STM32 断电 / CANH 断线 / 运行中删库 / 双板重启，均不崩、可恢复（S99can 自动）

## 已知限制

- **室内 GPS 未定位**：0x100/101 停发；开机自检曾残留测试坐标（sat=8），修复后 sat=0；论文不得引用自检坐标
- **RTC 时间依赖后备电池**：每次开测先 `date` 确认
- 0x301/0x302 在板倾斜/探头近物时会周期性触发（正常事件，非故障）
- 4h 历史数据中 `satellites=8` 属自检残留期采集，引用时需注明

## 仓库

https://github.com/WQZ107/08_linux_sensor_framework （私有）
