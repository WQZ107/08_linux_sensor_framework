/* src/thread_tcp.c —— TCP 推送线程（161-175 Day163；修复日一号 2026-10-28 加事件实时推送）
 * 职责：把 g_latest 橱窗里的最新成品 DataPoint + g_ev 事件橱窗，每秒打包成一行 JSON 推给 Qt。
 * 关键设计：
 *   1) 只读橱窗 g_latest/g_ev，不碰 g_databuf/g_procbuf（传送带的货属于 store，不能抢）；
 *   2) accept 用 poll 带 1 秒超时——Ctrl+C 置 g_running=0 后本线程最多 1~2 秒必退；
 *   3) send 带 MSG_NOSIGNAL：客户端断开返回 -1 走重连，不触发 SIGPIPE 杀进程；
 *   4) ★修复日：事件(0x300/301/302)也进推送——ev_seq 每来一个事件 +1，
 *      Qt 靠 seq 变化**立即弹窗**（此前事件只走 alert_log，要等 60s 同步窗，实测 35s）；
 *      alert_log 的 60s 节流只约束"历史记录"，不约束这里(实时推送不节流，Qt 去重)。
 */
#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

/* g_latest / g_latest_lock / g_ev / g_ev_lock 定义在 main.c，声明在 common.h */

void *tcp_thread(void *arg)
{
    (void)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[TCP] socket");
        return NULL;                       /* 降级：没有推送，其余线程照常 */
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(9000);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[TCP] bind");              /* 9000 被占=多半上个实例没退干净 */
        close(server_fd);
        return NULL;
    }

    listen(server_fd, 1);
    printf("[TCP] 监听 9000 端口，等待 Qt 连接...\n");

    while (g_running)
    {
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 1000);
        if (pr <= 0) continue;

        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;
        printf("[TCP] 客户端已连接\n");

        while (g_running)
        {
            /* 两个橱窗各拍一张快照(锁内只拷数据) */
            DataPoint dp;
            pthread_mutex_lock(&g_latest_lock);
            dp = g_latest;
            pthread_mutex_unlock(&g_latest_lock);

            EventInfo ev;
            pthread_mutex_lock(&g_ev_lock);
            ev = g_ev;
            pthread_mutex_unlock(&g_ev_lock);

            char json[400];
            int n = snprintf(json, sizeof(json),
                "{\"ts\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"spd\":%.0f,"
                "\"acc\":%.2f,\"temp\":%.1f,\"humi\":%.1f,\"P\":%.1f,\"R\":%.1f,"
                "\"dist\":%.0f,\"sat\":%d,\"alert\":%d,"
                "\"ev_seq\":%d,\"ev_type\":%d,\"ev_val\":%d,\"ev_p10\":%d}\n",
                dp.timestamp, dp.lat, dp.lon, dp.speed,
                dp.accel_total, dp.temperature, dp.humidity,
                dp.pitch, dp.roll, dp.distance,
                dp.satellites, dp.alert,
                ev.seq, ev.type, ev.val, ev.p10);

            if (send(client, json, n, MSG_NOSIGNAL) <= 0) {
                printf("[TCP] 客户端断开，等待重连\n");
                break;
            }
            sleep(1);
        }
        close(client);
    }

    close(server_fd);
    printf("[TCP] 退出\n");
    return NULL;
}
