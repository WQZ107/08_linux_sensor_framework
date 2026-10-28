/* src/thread_can.c —— CAN 接收线程（版本B：重构后，单一分派）
 * 与版本A的差别：解析器直接写共享区（锁内），线程里不再有第二遍 switch。
 * 事件帧只打印；★组6 补（2026-10-24）：事件同时写 alert_log（同类 60 秒节流）。
 * 加固（2026-10-20）：read 遇到 ENETDOWN 等不退出，限速打印 + 每秒重试。
 */
#include "common.h"
#include "thread_can.h"
#include "can_linux.h"
#include "can_parser.h"
#include "database.h"      /* ★组6：db_insert_alert */

#include <errno.h>
#include <linux/can.h>
#include <time.h>          /* ★组6：strftime/localtime/节流 */

extern DBHandle g_db;                      /* ★组6：main.c 定义的数据库句柄 */
extern pthread_mutex_t g_db_lock;          /* ★组6：写库互斥锁（与存储线程同一把） */

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

        /* 事件帧：打印 + ★组6 写 alert_log（同类 60 秒节流） */
        if (id >= 0x300 && id <= 0x302) {
            CanData ev;
            memset(&ev, 0, sizeof(ev));
            if (can_parse_frame(&frame, &ev) == 0) {
			                /* ★修复日:事件进橱窗(实时推送不节流;60s 节流只约束 alert_log 历史记录) */
                pthread_mutex_lock(&g_ev_lock);
                g_ev.seq++;
                if (ev.event_id == 0x300)      { g_ev.type = 1; g_ev.val = ev.event_val; g_ev.p10 = 0; }
                else if (ev.event_id == 0x301) { g_ev.type = 2; g_ev.val = ev.event_r10;  g_ev.p10 = ev.event_p10; }
                else                           { g_ev.type = 3; g_ev.val = ev.event_val; g_ev.p10 = 0; }
                pthread_mutex_unlock(&g_ev_lock);
                if (ev.event_id == 0x300)
                    printf("[CAN线程] 事件 急刹 acc=%.2fg\n", ev.event_val / 100.0);
                else if (ev.event_id == 0x301)
                    printf("[CAN线程] 事件 侧翻 P=%.1f R=%.1f\n",
                           ev.event_p10 / 10.0, ev.event_r10 / 10.0);
                else if (ev.event_id == 0x302)
                    printf("[CAN线程] 事件 防撞 dist=%dmm\n", ev.event_val);

                /* ★组6：同类事件 60 秒内只记一条（防持续遮挡把同步/弹窗灌爆） */
                {
                    static time_t last_ev_ts[3] = {0, 0, 0};
                    int ev_idx = ev.event_id - 0x300;
                    time_t now = time(NULL);
                    if (ev_idx >= 0 && ev_idx < 3 &&
                        now - last_ev_ts[ev_idx] >= 60) {
                        char ts[32], detail[64];
                        const char *type;
                        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S",
                                 localtime(&now));
                        if (ev.event_id == 0x300) {
                            type = "急刹";
                            snprintf(detail, sizeof(detail), "%.2fg",
                                     ev.event_val / 100.0);
                        } else if (ev.event_id == 0x301) {
                            type = "侧翻";
                            snprintf(detail, sizeof(detail), "P=%.1f R=%.1f",
                                     ev.event_p10 / 10.0, ev.event_r10 / 10.0);
                        } else {
                            type = "防撞";
                            snprintf(detail, sizeof(detail), "dist=%dmm",
                                     ev.event_val);
                        }
                        pthread_mutex_lock(&g_db_lock);  /* 同一连接不许多线程同时写 */
                        db_insert_alert(&g_db, ts, type, detail);
                        pthread_mutex_unlock(&g_db_lock);
                        last_ev_ts[ev_idx] = now;
                        printf("[CAN线程] 告警入库: %s %s\n", type, detail);
                    }
                }
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
