/* src/thread_gps.c —— GPS 接收线程（串口读NMEA → 拼行 → 加锁解析进 g_gps） */
#include "common.h"
#include "gps_nmea.h"
#include "serial.h"

void *gps_thread(void *arg)
{
    (void)arg;
    int fd = serial_open("/dev/ttymxc2", 9600);   /* 块4定的设备名：UART3/GPS接口 */
    if (fd < 0) {
        fprintf(stderr, "[GPS线程] 串口打开失败（查JP2接线与设备名）\n");
        return NULL;
    }
    printf("[GPS线程] 启动\n");

    char buf[256], line[128];
    int line_len = 0;

    while (g_running)
    {
        int len = read(fd, buf, sizeof(buf) - 1);  /* VMIN=0/VTIME=10：最多1秒返回 */
        if (len <= 0) { usleep(10000); continue; }

        for (int i = 0; i < len && g_running; i++)
        {
            if (buf[i] == '\n') {                  /* 一行结束 */
                line[line_len] = '\0';
                pthread_mutex_lock(&g_gps_lock);   /* ★写共享区前加锁 */
                GPS_ParseLine(line);               /* 块4的解析器，原样用 */
                pthread_mutex_unlock(&g_gps_lock); /* ★用完立刻解锁 */
                line_len = 0;
            } else if (buf[i] != '\r') {
                if (line_len < (int)sizeof(line) - 1)
                    line[line_len++] = buf[i];     /* 防越界 */
            }
        }
    }
    close(fd);
    printf("[GPS线程] 退出\n");
    return NULL;
}
