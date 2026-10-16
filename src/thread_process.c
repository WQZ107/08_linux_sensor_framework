/* src/thread_process.c —— 处理线程（第3步④：第一个消费者——阻塞等数据、告警检测、周期打印） */
#include "common.h"
#include "ringbuf.h"          /* ⭐必须有：rb_pop 的声明在这里（引用完整性自查：rb_pop→ringbuf.h；g_databuf/DataPoint→common.h）*/

void *process_thread(void *arg)
{
    (void)arg;
    printf("[处理线程] 启动\n");
    DataPoint dp;
    int print_count = 0;

    while (g_running)
    {
        rb_pop(&g_databuf, &dp);              /* 阻塞等数据：没数据就睡在 not_empty 床上 */

        if (dp.speed > 100) {                 /* 告警检测（阈值到 126-140 按 CAN 真值再调） */
            dp.alert = 1;
            printf("[ALERT] [!] 超速! %.0f km/h\n", dp.speed);
        }
        if (dp.accel_total > 1.8f) {
            dp.alert = 1;
            printf("[ALERT] [!] 剧烈运动! %.2fg\n", dp.accel_total);
        }
	rb_push(&g_procbuf, &dp);    /* B方案：处理完转交2号buffer（每条都"既处理又存储"）*/

        if (++print_count % 10 == 0) {        /* 每10条(约10秒)一次,防刷屏 */

            printf("[DATA] lat=%.6f lon=%.6f spd=%.0f accel=%.2fg temp=%.1f humi=%.1f P=%.1f R=%.1f D=%.0f valid=%d %s\n",
                   dp.lat, dp.lon, dp.speed, dp.accel_total,
                   dp.temperature, dp.humidity, dp.pitch, dp.roll, dp.distance,
                   dp.valid, dp.alert ? "ALERT" : "");
        }
    }
    printf("[处理线程] 退出\n");
    return NULL;
}
