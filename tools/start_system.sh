#!/bin/sh
# /home/root/start_system.sh —— 车载数据记录系统·板端一键启动(161-175 Day166)
# v3 (2026-10-28):
#   v2 修的"非交互式 ssh PATH 精简(/sbin 缺失)"→ 已由顶部 export PATH 解决(实测 ip 可用)
#   v2 引入的新坑: CAN 状态改用 sysfs operstate 检查 —— **CAN 的 operstate 常为 unknown**,
#     不代表 up → 误报"未 UP"。v3 回到 `ip link show` 看 flags 里的 UP(V2 时 PATH 已补齐,ip 可用)
# 配套(两半的另一半): 【VM】~/start_work.sh (同步) + 启动 Qt
# 设计原则: 每步"先查后做",已就绪则跳过; 单实例保护; 每步打判据

export PATH=/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin
# TZ 统一(修复日 2026-10-28 定案方案A): 程序 localtime() 继承启动者 TZ——
# 交互式 shell 有 TZ=CST-8、非交互式 ssh 没有 → 数据库时间戳会混排;在此钉死北京时间
export TZ=CST-8

APP=/home/root/sensor_framework
LOG=/tmp/system.log
ETH=eth1
IP=192.168.1.100

echo "=== 车载数据记录系统·板端启动 ==="

# 1. CAN(Day126 学的): S99can 开机已自动 UP/500k
#    检查看 ip link 的 flags(UP/LOWER_UP); 坏了才重配三连
if ip link show can0 2>/dev/null | grep -q "UP"; then
    echo "[CAN] can0 已 UP ✅"
else
    echo "[CAN] can0 未 UP,重配三连..."
    ip link set can0 down
    ip link set can0 type can bitrate 500000
    ip link set can0 up
    sleep 1
    if ip link show can0 2>/dev/null | grep -q "UP"; then
        echo "[CAN] 重配成功 ✅"
    else
        echo "[CAN] 启动失败 ❌ 查 S99can / CAN 接线"
    fi
fi

# 2. 网络(Day73 学的): S99static-ip 开机自动钉 IP,这里确认
if ifconfig $ETH 2>/dev/null | grep -q "$IP"; then
    echo "[NET] $ETH = $IP ✅"
else
    echo "[NET] $ETH 没钉上 $IP,尝试手动钉..."
    ifconfig $ETH $IP netmask 255.255.255.0 up 2>/dev/null
    if ifconfig $ETH 2>/dev/null | grep -q "$IP"; then
        echo "[NET] 手动钉成功 ✅"
    else
        echo "[NET] 失败 ❌ 查 /etc/rc5.d/S99static-ip 与网线"
    fi
fi

# 3. 单实例保护(2026-10-26 实录: 双实例=bind 冲突+双写+I2C 互掐)
OLD=$(pidof sensor_framework)
if [ -n "$OLD" ]; then
    echo "[APP] 发现旧实例 PID=$OLD,优雅停止..."
    kill -INT $OLD
    sleep 2
    OLD=$(pidof sensor_framework)
    if [ -n "$OLD" ]; then
        echo "[APP] 旧实例仍未退,强停 $OLD"
        kill -9 $OLD
        sleep 1
    fi
    echo "[APP] 旧实例已清 ✅"
else
    echo "[APP] 无旧实例 ✅"
fi

# 4. 启动采集(Day99 学的: 后台+日志重定向+nohup 免疫断线)
nohup $APP > $LOG 2>&1 &
NEWPID=$!
echo "[APP] 采集已启动 PID=$NEWPID"
sleep 3

# 5. 判据自检(不看判据不算启动完)
if pidof sensor_framework >/dev/null; then
    echo "[APP] 进程存活 ✅"
else
    echo "[APP] 启动失败 ❌ 看 $LOG"
fi
if grep "\[TCP\] 监听" $LOG | head -1; then
    echo "[TCP] 监听就绪 ✅"
else
    echo "[TCP] 未见监听行 ⚠️ 看日志(若 bind 被占=还有别的实例)"
fi
if grep "\[CAN\]" $LOG | head -1 >/dev/null 2>&1; then
    echo "[CAN] 数据已进 ✅"
else
    echo "[CAN] 暂无数据 ⚠️ 等 1~2 秒 tail $LOG 再看(STM32 在发吗?)"
fi

echo "=== 板端启动完成 ==="
echo "下一步: 【VM】~/start_work.sh (同步) + 启动 Qt,30 秒内应看到数"
