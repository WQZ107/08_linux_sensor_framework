/* src/collector.c —— 采集组装线程（生产者：每秒拼一条 DataPoint 推入1号buffer） */
#include "common.h"
#include "ringbuf.h"          /* 引用完整性自查：rb_push→ringbuf.h */

void *collector_thread(void *arg)
{
    (void)arg;
    printf("[采集线程] 启动，每1秒组装一条\n");
    DataPoint dp;

    while (g_running)
    {
        get_timestamp(dp.timestamp, sizeof(dp.timestamp));   /* Day3 的函数 */

        pthread_mutex_lock(&g_gps_lock);                     /* ── 拷 GPS（锁内只拷数据）── */
        dp.lat   = g_gps.latitude;
        dp.lon   = g_gps.longitude;
        dp.speed = g_gps.speed_kmh;
        dp.valid = g_gps.valid;
        pthread_mutex_unlock(&g_gps_lock);

        pthread_mutex_lock(&g_accel_lock);                   /* ── 拷加速度 ── */
        dp.accel_total = g_accel.total;
        pthread_mutex_unlock(&g_accel_lock);

        dp.temperature = 28.0f + (rand() % 20) / 10.0f;      /* 模拟值（真值 126-140 起走 CAN） */
        dp.humidity    = 60.0f + (rand() % 30) / 10.0f;
        dp.pitch       = (rand() % 60) / 10.0f - 3.0f;
        dp.roll        = (rand() % 100) / 10.0f - 5.0f;
        dp.distance    = 300 + (rand() % 1500);
        dp.alert       = 0;

        rb_push(&g_databuf, &dp);                            /* 交棒1号buffer：满了会睡 */
        usleep(1000000);                                     /* 1 秒一条 */
    }
    printf("[采集线程] 退出\n");
    return NULL;
}
