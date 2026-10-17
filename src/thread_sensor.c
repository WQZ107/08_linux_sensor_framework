/* src/thread_sensor.c —— 传感器线程（第5步：真 MPU6050；读不到就降级模拟） */
#include "common.h"
#include "mpu6050_linux.h"
#include <math.h>

void *sensor_thread(void *arg)
{
    (void)arg;
    int real_ok = (MPU6050_Linux_Init() == 0);
    if (!real_ok)
        printf("[传感器线程] MPU6050 不可用，降级为模拟数据（活着比完美重要）\n");

    srand(time(NULL));

    while (g_running)
    {
        float ax, ay, az;

        if (real_ok && MPU6050_Linux_ReadAccel(&ax, &ay, &az) == 0) {
            pthread_mutex_lock(&g_accel_lock);        /* 真值：从 I2C 读来的 */
            g_accel.ax = ax;  g_accel.ay = ay;  g_accel.az = az;
            g_accel.total = sqrtf(ax*ax + ay*ay + az*az);
            pthread_mutex_unlock(&g_accel_lock);
        } else {
            pthread_mutex_lock(&g_accel_lock);        /* 降级：模拟值,程序照常活着 */
            g_accel.ax = (rand() % 200 - 100) / 1000.0f;
            g_accel.ay = (rand() % 200 - 100) / 1000.0f;
            g_accel.az = 0.95f + (rand() % 100) / 1000.0f;
            g_accel.total = sqrtf(g_accel.ax*g_accel.ax +
                                  g_accel.ay*g_accel.ay +
                                  g_accel.az*g_accel.az);
            pthread_mutex_unlock(&g_accel_lock);
        }
        usleep(100000);   /* 100ms 一次 */
    }
    printf("[传感器线程] 退出\n");
    return NULL;
}
