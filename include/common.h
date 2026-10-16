/* include/common.h —— 全项目公共定义（本工程"宪法"） */
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
    int    valid;            /* GPS定位有效？（本工程比教材多加1列：室内也能验全链） */
    int    alert;            /* 0=正常 1=告警 */
} DataPoint;

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

/* ─── 工具函数（main.c 实现）─── */
void get_timestamp(char *buf, int size);

#endif /* __COMMON_H__ */
