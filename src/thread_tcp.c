/* src/thread_tcp.c —— TCP 推送线程（161-175 Day163）
 * 职责：把 g_latest 橱窗里的最新成品 DataPoint，每秒打包成一行 JSON 推给 Qt。
 * 关键设计：
 *   1) 只读橱窗 g_latest，不碰 g_databuf/g_procbuf（传送带的货属于 store，不能抢）；
 *   2) accept 用 poll 带 1 秒超时——Ctrl+C 置 g_running=0 后本线程最多 1~2 秒必退，
 *      保住"七线程干净退出"的家底（教材裸 accept 会睡死，join 卡住）；
 *   3) send 带 MSG_NOSIGNAL：客户端断开返回 -1 走重连，不触发 SIGPIPE 杀进程。
 */
#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

/* g_latest / g_latest_lock 定义在 main.c，声明在 common.h */

void *tcp_thread(void *arg)
{
    (void)arg;

    /* ── 五步曲前两步：socket + bind ── */
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
    addr.sin_addr.s_addr = INADDR_ANY;     /* 0.0.0.0 = 任意网卡来的都收 */
    addr.sin_port        = htons(9000);    /* host to network short：端口字节序 */
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[TCP] bind");              /* 9000 被占=多半上个实例没退干净 */
        close(server_fd);
        return NULL;
    }

    /* ── 第三步：listen ── */
    listen(server_fd, 1);
    printf("[TCP] 监听 9000 端口，等待 Qt 连接...\n");

    while (g_running)
    {
        /* ── 第四步：accept，但用 poll 等 1 秒就回来看一眼 g_running ── */
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 1000);
        if (pr <= 0) continue;             /* 超时或被信号打断：回去查退出开关 */

        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;
        printf("[TCP] 客户端已连接\n");

        while (g_running)
        {
            /* 从橱窗拍快照：整结构体复制一份，锁外慢慢发 */
            DataPoint dp;
            pthread_mutex_lock(&g_latest_lock);
            dp = g_latest;
            pthread_mutex_unlock(&g_latest_lock);

            /* ── 第五步：send（一行 JSON，\n 结尾=Qt canReadLine 的切分标记）── */
            char json[320];
            int n = snprintf(json, sizeof(json),
                "{\"ts\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"spd\":%.0f,"
                "\"acc\":%.2f,\"temp\":%.1f,\"humi\":%.1f,\"P\":%.1f,\"R\":%.1f,"
                "\"dist\":%.0f,\"sat\":%d,\"alert\":%d}\n",
                dp.timestamp, dp.lat, dp.lon, dp.speed,
                dp.accel_total, dp.temperature, dp.humidity,
                dp.pitch, dp.roll, dp.distance,
                dp.satellites, dp.alert);

            if (send(client, json, n, MSG_NOSIGNAL) <= 0) {
                printf("[TCP] 客户端断开，等待重连\n");
                break;                     /* 回外层 accept 等它回来 */
            }
            sleep(1);                      /* 每秒一条，与采集节拍一致 */
        }
        close(client);
    }

    close(server_fd);
    printf("[TCP] 退出\n");
    return NULL;
}
