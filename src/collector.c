/* src/collector.c —— 采集组装线程（组5：数据源 = CAN 共享区）
 * 组5 的改动：原来读本地 g_gps + g_accel + 模拟温湿度/姿态/距离，
 *             现在只读 CAN_GetData()；DataPoint 的 satellites 来自 CAN 0x107。
 * 说明：本地 GPS/MPU 线程仍保留（自检/降级），但不参与入库。
 */
#include "common.h"
#include "ringbuf.h"
#include "thread_can.h"        /* CAN_GetData + CanData */
#include <math.h>

void *collector_thread(void *arg)
{
    (void)arg;
    printf("[采集线程] 启动（数据源：CAN）\n");
    DataPoint dp;

    while (g_running)
    {
        CanData cd;
        CAN_GetData(&cd);                        /* ★组5：就换了这一行的来源 */

        get_timestamp(dp.timestamp, sizeof(dp.timestamp));
        dp.lat         = cd.lat;
        dp.lon         = cd.lon;
        dp.speed       = cd.speed;
        dp.temperature = cd.temp_x10 / 10.0f;     /* 0x103 ×10 */
        dp.humidity    = cd.humi_x10 / 10.0f;     /* 0x108 ×10 */
        dp.pitch       = cd.pitch10 / 10.0f;      /* 0x109 ×10 */
        dp.roll        = cd.roll10 / 10.0f;
        dp.distance    = (float)cd.dist_mm;       /* 0x10A mm 原生 */

        /* 合加速度：0x104~106 单位 0.01g → ÷100 得 g */
        float ax = cd.ax100 / 100.0f;
        float ay = cd.ay100 / 100.0f;
        float az = cd.az100 / 100.0f;
        dp.accel_total = sqrtf(ax * ax + ay * ay + az * az);

        dp.valid      = (cd.sat > 0) ? 1 : 0;     /* 有卫星数视为已定位（假坐标问题见 03） */
        dp.satellites = cd.sat;                   /* ★组5：0x107 data[0] */
        dp.alert      = 0;                        /* 告警交给 process 线程判定 */

        rb_push(&g_databuf, &dp);                 /* 交棒1号buffer */
        usleep(1000000);                          /* 1 秒一条 */
    }
    printf("[采集线程] 退出\n");
    return NULL;
}
