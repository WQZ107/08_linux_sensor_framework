/* src/thread_process.c —— 处理线程（④：消费者+生产者——1号buffer取数、告警检测、转交2号buffer） */
#include "common.h"
#include "ringbuf.h"

void *process_thread(void *arg)
{
    (void)arg;
    printf("[处理线程] 启动\n");
    DataPoint dp;
    int print_count = 0;

    while (g_running)
    {
        /* ★第4步：带超时地等——最多睡1秒就回来检查 g_running，Ctrl+C 才退得出 */
        if (rb_pop_timeout(&g_databuf, &dp, 1) == 0)
        {
            if (dp.speed > 100) {                 /* 告警检测 */
                dp.alert = 1;
                printf("[ALERT] [!] 超速! %.0f km/h\n", dp.speed);
            }
            if (dp.accel_total > 1.8f) {
                dp.alert = 1;
                printf("[ALERT] [!] 剧烈运动! %.2fg\n", dp.accel_total);
            }

            rb_push(&g_procbuf, &dp);             /* B方案：处理完转交2号buffer */

            if (++print_count % 10 == 0) {        /* 每10条一次,防刷屏 */
                printf("[DATA] lat=%.6f lon=%.6f spd=%.0f accel=%.2fg temp=%.1f humi=%.1f P=%.1f R=%.1f D=%.0f valid=%d %s\n",
                       dp.lat, dp.lon, dp.speed, dp.accel_total,
                       dp.temperature, dp.humidity, dp.pitch, dp.roll, dp.distance,
                       dp.valid, dp.alert ? "ALERT" : "");
            }
        }
        /* 超时就回 while 看 g_running，能正常退出 */
    }
    printf("[处理线程] 退出\n");
    return NULL;
}
