/* src/thread_sensor.c —— 传感器线程（模拟数据；第5步换真 MPU6050） */
#include "common.h"
#include <math.h>

void *sensor_thread(void *arg)
{
    (void)arg;
    printf("[传感器线程] 启动（模拟模式）\n");
    srand(time(NULL));

    while (g_running)
    {
        pthread_mutex_lock(&g_accel_lock);               /* 进门前锁 */
        g_accel.ax = (rand() % 200 - 100) / 1000.0f;     /* -0.1 ~ 0.1 g */
        g_accel.ay = (rand() % 200 - 100) / 1000.0f;
        g_accel.az = 0.95f + (rand() % 100) / 1000.0f;   /* ≈1g 重力 */
        g_accel.total = sqrtf(g_accel.ax * g_accel.ax +
                              g_accel.ay * g_accel.ay +
                              g_accel.az * g_accel.az);
        pthread_mutex_unlock(&g_accel_lock);             /* 出门解锁 */

        usleep(100000);   /* 100ms 采一次 */
    }
    printf("[传感器线程] 退出\n");
    return NULL;
}
