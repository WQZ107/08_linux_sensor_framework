/* src/thread_can.c —— CAN 接收线程（版本B：重构后，单一分派）
 * 与版本A的差别：解析器直接写共享区（锁内），线程里不再有第二遍 switch。
 * 事件帧仍然只打印（不进共享区）。
 * 加固（2026-10-20）：read 遇到 ENETDOWN 等不退出，限速打印 + 每秒重试。
 */
#include "common.h"
#include "thread_can.h"
#include "can_linux.h"
#include "can_parser.h"

#include <errno.h>
#include <linux/can.h>

static CanData g_can;                                   /* CAN 共享区 */
static pthread_mutex_t g_can_lock = PTHREAD_MUTEX_INITIALIZER;

void CAN_GetData(CanData *out)
{
    if (out == NULL) return;
    pthread_mutex_lock(&g_can_lock);
    *out = g_can;                                       /* 锁内只拷数据 */
    pthread_mutex_unlock(&g_can_lock);
}

static void print_latest(const CanData *d)
{
    printf("[CAN] lat=%.6f lon=%.6f spd=%d T=%.1f H=%.1f P=%.1f R=%.1f D=%d sat=%d\n",
           d->lat, d->lon, d->speed,
           d->temp_x10 / 10.0f, d->humi_x10 / 10.0f,
           d->pitch10 / 10.0f, d->roll10 / 10.0f,
           d->dist_mm, d->sat);
}

void *can_thread(void *arg)
{
    (void)arg;
    struct can_frame frame;
    int n;
    time_t last_print = 0;
    time_t last_err_print = 0;

    printf("[CAN线程] 启动\n");

    while (g_running)
    {
        n = read(can_linux_getfd(), &frame, sizeof(frame));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;  /* 1s 超时 */
            if (errno == EBADF) break;

            /* ENETDOWN/ENOBUFS 等：不退出，限速打印 + 每秒重试 */
            time_t now_err = time(NULL);
            if (now_err != last_err_print) {
                last_err_print = now_err;
                perror("[CAN线程] read（can0 没 up？每秒重试中）");
            }
            sleep(1);
            continue;
        }
        if (n != sizeof(frame)) continue;

        uint32_t id = frame.can_id & 0x7FF;

        /* 事件帧：只打印 */
        if (id >= 0x300 && id <= 0x302) {
            CanData ev;
            memset(&ev, 0, sizeof(ev));
            if (can_parse_frame(&frame, &ev) == 0) {
                if (ev.event_id == 0x300)
                    printf("[CAN线程] 事件 急刹 acc=%.2fg\n", ev.event_val / 100.0);
                else if (ev.event_id == 0x301)
                    printf("[CAN线程] 事件 侧翻 P=%.1f R=%.1f\n",
                           ev.event_p10 / 10.0, ev.event_r10 / 10.0);
                else if (ev.event_id == 0x302)
                    printf("[CAN线程] 事件 防撞 dist=%dmm\n", ev.event_val);
            }
            continue;
        }

        /* ★版本B：解析器直接写共享区——只有一次分发，锁只加一次 */
        pthread_mutex_lock(&g_can_lock);
        int r = can_parse_frame(&frame, &g_can);
        CanData snapshot = g_can;
        pthread_mutex_unlock(&g_can_lock);
        if (r != 0) continue;

        time_t now = time(NULL);
        if (now != last_print) {
            last_print = now;
            print_latest(&snapshot);
        }
    }

    printf("[CAN线程] 退出\n");
    return NULL;
}
