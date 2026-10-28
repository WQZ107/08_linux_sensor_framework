/* include/common.h —— 全项目公共定义（本工程"宪法"）
 * 组5：DataPoint 新增 satellites（卫星数由 CAN 0x107 填入） */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "gps_nmea.h"      /* GPS_Data + extern g_gps —— g_gps 定义在 gps_nmea.c（解析器心脏不动） */

/* ─── 传感器数据结构 ─── */

/* 加速度数据（传感器线程写，采集线程读） */
typedef struct {
    float ax, ay, az;        /* 单位 g */
    float total;             /* 合加速度 */
} AccelData;

/* 综合数据点（buffer 里装的就是它） */
typedef struct {
    char   timestamp[20];
    double lat, lon;
    float  speed;
    float  accel_total;
    float  temperature;
    float  humidity;         /* %RH  （块K：STM32 端 DHT11，126-140 起由 CAN 0x108 真值替换） */
    float  pitch;            /* 俯仰角°（块L：姿态解算 0x109） */
    float  roll;             /* 横滚角°（块L） */
    float  distance;         /* 距离mm（块M：HC-SR04 0x10A） */
    int    valid;            /* GPS定位有效？（组5：由 CAN 卫星数推导） */
    int    satellites;       /* 卫星数（组5：CAN 0x107 data[0]，原来在 store 线程从 g_gps 取） */
    int    alert;            /* 0=正常 1=告警 */
} DataPoint;

/* 事件橱窗(修复日:事件实时推送用;thread_can 写,TCP 读) */
typedef struct {
	int seq;		/* 事件序号:每来一个事件帧 +1,Qt 靠它判"新事件" */
	int type;		/* 1=急刹 2=侧翻 3=防撞 */
	int val;		/* 急刹=acc×100 / 侧翻=R×10 / 防撞=dist mm */
	int p10;		/* 侧翻专用:P×10(其余为 0) */
} EventInfo;

/* ─── 线程安全环形 buffer ─── */
typedef struct {
    DataPoint *buffer;
    int head, tail, size;
    pthread_mutex_t lock;        /* buffer 的锁 */
    pthread_cond_t  not_empty;   /* 条件变量：有数据了 */
    pthread_cond_t  not_full;    /* 条件变量：有空位了 */
} DataRingBuffer;

/* ─── 全局共享资源声明（定义放 main.c；g_gps 例外：在 gps_nmea.c）─── */
extern pthread_mutex_t g_gps_lock;
extern AccelData g_accel;
extern pthread_mutex_t g_accel_lock;
extern DataRingBuffer g_databuf;
extern DataRingBuffer g_procbuf;
extern volatile int g_running;   /* ⭐退出开关 */
extern DataPoint g_latest;		/* Day163:最新成品橱窗（process 盖章后更新，TCP只看不动）*/
extern pthread_mutex_t g_latest_lock;
extern EventInfo g_ev;		  /* 修复日:事件橱窗 */
extern pthread_mutex_t g_ev_lock;

/* ─── 工具函数（main.c 实现）─── */
void get_timestamp(char *buf, int size);

#endif /* __COMMON_H__ */
