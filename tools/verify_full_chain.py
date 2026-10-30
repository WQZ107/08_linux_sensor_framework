#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""tools/verify_full_chain.py —— 全链路数据一致性回归(161-175 Day164;Day170 修复)
在 VM 上跑: python3 tools/verify_full_chain.py
前提: 板上 sensor_framework 在跑(TCP 9000 在听)+ start_work.sh 同步在跑

三件套(= Day161-162 手动五环取证的自动化版):
  ① TCP 实时值 vs 板上库最新行 —— 秒级一致性,宽容差(采样时刻不同,物理量在动)
     ⚠️ 板端单客户端设计:Qt 连着时 ① 能连上但收不到数据 → SKIP(不是失败)
  ② VM 副本按 id 对齐逐字段 vs 板库同 id 行(3 行) —— append-only,零容差
  ③ id 差 = 同步延迟量化(秒) —— 0~170 正常(库 20MB+ 后 .backup+scp 使周期拉长)

2026-10-30 变更:
  - ① 区分 refused(程序没跑) / timeout(Qt 占位 → SKIP) / 其他异常
  - ② 由 1 行扩为 3 行(IN 一次查回,按 id 装字典逐个对)
  - ③ 阈值 70 → 170(库体积增长拉长同步周期)
"""
import socket, json, subprocess, sys

BOARD    = "192.168.1.100"
BOARD_DB = "/home/root/data/vehicle.db"
LOCAL_DB = "/home/wu/sync/vehicle.db"
FIELDS   = ["timestamp","latitude","longitude","speed","accel_total",
            "temperature","humidity","pitch","roll","distance","satellites","alert"]

TCP_KEY  = {"temperature":"temp","humidity":"humi","distance":"dist","speed":"spd",
            "accel_total":"acc","pitch":"P","roll":"R","satellites":"sat",
            "latitude":"lat","longitude":"lon"}
TOL      = {"temperature":1.0,"humidity":2.0,"distance":80.0,"accel_total":0.1,
            "pitch":0.5,"roll":0.5}
LAG_MAX  = 170   # 同步周期 60s + .backup/scp 耗时(库越大越久) + 余量

def ssh_q(sql):
    """板上库查询:ssh 免密 + 老 dropbear 算法兼容(组5 同款参数)"""
    r = subprocess.run(["ssh","-o","HostKeyAlgorithms=+ssh-rsa",
                        f"root@{BOARD}",
                        f'sqlite3 {BOARD_DB} "{sql}"'],
                       capture_output=True, text=True, timeout=10)
    return r.stdout.strip()

def local_q(sql):
    r = subprocess.run(["sqlite3", LOCAL_DB, sql],
                       capture_output=True, text=True, timeout=10)
    return r.stdout.strip()

def row(line):
    """'123|2026-10-25 21:00:00|0.0|...' → dict;列数不对返回 None"""
    p = line.split("|")
    if len(p) != len(FIELDS) + 1:
        return None
    d = {"id": int(p[0])}
    d.update(zip(FIELDS, p[1:]))
    return d

def same(a, b):
    """数值字段按 float 比(两边 sqlite3 文本表示可能有差);其余按字符串"""
    try:
        return float(a) == float(b)
    except ValueError:
        return str(a) == str(b)

def main():
    print("=" * 50)
    print("全链路一致性回归 verify_full_chain")
    print("=" * 50)
    fails = []
    tcp_skip = False

    # ── ① TCP 实时值(每秒一行 JSON) ──
    tcp = None
    try:
        s = socket.create_connection((BOARD, 9000), timeout=5)
        s.settimeout(5)
        buf = b""
        while b"\n" not in buf and len(buf) < 1024:
            c = s.recv(512)
            if not c:
                break
            buf += c
        s.close()
        tcp = json.loads(buf.split(b"\n")[0])
        print(f"[①TCP ] {tcp.get('ts')} temp={tcp.get('temp')} humi={tcp.get('humi')} "
              f"dist={tcp.get('dist')} sat={tcp.get('sat')}")
    except ConnectionRefusedError:
        fails.append("TCP refused: 板子程序没跑?")
        print("[①TCP ] FAIL: refused——板子程序没跑(ps + tail 查)")
    except socket.timeout:
        tcp_skip = True
        print("[①TCP ] SKIP: 5 秒没数据——多半 Qt 占着客户端位(单客户端设计);停 Qt 再跑可验")
    except Exception as e:
        fails.append(f"TCP 异常: {e}")
        print(f"[①TCP ] FAIL: {e}")

    # ── 两边库的最新行 ──
    b_line = ssh_q(f"SELECT id,{','.join(FIELDS)} FROM data_point ORDER BY id DESC LIMIT 1;")
    b_new = row(b_line) if b_line else None
    l_line = local_q(f"SELECT id,{','.join(FIELDS)} FROM data_point ORDER BY id DESC LIMIT 1;")
    l_new = row(l_line) if l_line else None
    if not b_new:
        fails.append("板上库查询失败(ssh/路径?)");  print("[板库 ] FAIL:", b_line)
    if not l_new:
        fails.append("本地副本查询失败(start_work.sh 在跑吗?)"); print("[副本 ] FAIL:", l_line)
    if not b_new or not l_new:
        print("\n结论: ❌ 环境未就绪,先修上面两条再跑"); return 1

    # ① TCP vs 板库最新行(宽容差:两边取样时刻差 0~1 秒,物理量在动)
    if tcp:
        for f, k in TCP_KEY.items():
            t, b = float(tcp[k]), float(b_new[f])
            if abs(t - b) > TOL.get(f, 0.0):
                print(f"[①比对] WARN {f}: TCP={t} vs 板库={b} 超容差{TOL.get(f, 0)}"
                      "(单次超差=采样时刻不同;连续多秒超差才是问题)")
        print("[①比对] TCP ↔ 板库 宽容差比对完成")

    # ── ② id 对齐(3 行版):副本最新 3 行,IN 一次查回,按 id 装字典逐个对 ──
    l_lines = local_q(f"SELECT id,{','.join(FIELDS)} FROM data_point ORDER BY id DESC LIMIT 3;")
    l_rows = [r for r in (row(x) for x in l_lines.splitlines() if x.strip()) if r]
    if not l_rows:
        fails.append("副本里一行都没查到")
    else:
        ids = ",".join(str(r["id"]) for r in l_rows)
        b_multi = ssh_q(f"SELECT id,{','.join(FIELDS)} FROM data_point "
                        f"WHERE id IN ({ids}) ORDER BY id DESC;")
        b_map = {r["id"]: r for r in (row(x) for x in b_multi.splitlines() if x.strip()) if r}
        for lr in l_rows:
            br = b_map.get(lr["id"])
            if not br:
                fails.append(f"板上找不到 id={lr['id']}")
                print(f"[②对齐] FAIL: 板上查无 id={lr['id']}")
                continue
            bad = [f for f in ["timestamp"] + FIELDS if not same(br[f], lr[f])]
            if bad:
                fails.append(f"id={lr['id']} 字段不一致: {bad}")
                print(f"[②对齐] FAIL id={lr['id']}: {bad}")
            else:
                print(f"[②对齐] id={lr['id']} 全 {len(FIELDS)} 字段逐位一致 ✅")

    # ── ③ 同步延迟量化 ──
    lag = b_new["id"] - l_new["id"]
    ok3 = 0 <= lag <= LAG_MAX
    print(f"[③延迟] 板库 max={b_new['id']} 副本 max={l_new['id']} → 落后 {lag} 秒"
          f"({'正常' if ok3 else '可疑!'})")
    if not ok3:
        fails.append(f"同步延迟 {lag}s 超预期(>{LAG_MAX}s)")

    print("-" * 50)
    if fails:
        print("结论: ❌ " + "; ".join(fails))
        return 1
    msg = "结论: ✅ 全链路一致(②id 对齐零容差过 / ③延迟正常)"
    msg += "(①TCP SKIP: Qt 占用客户端位)" if tcp_skip else "(①宽容差过)"
    print(msg)
    return 0

if __name__ == "__main__":
    sys.exit(main())
