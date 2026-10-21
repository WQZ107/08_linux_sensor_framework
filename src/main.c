/* src/main.c —— 程序入口：初始化共享资源、创建 5+1 个线程、优雅退出
   （v1.2 组4：加入 CAN 接收线程；CAN 初始化失败时其他线程照常运行） */
#include "common.h"
#include "ringbuf.h"
#include <signal.h>
#include "database.h"          /* 组3：SQLite database 模块 */
#include "can_linux.h"         /* 组4：CAN 套接字初始化 */
#include "thread_can.h"        /* 组4：can_thread 原型 */

/* ─── 全局共享资源定义（common.h 里 extern 声明过；g_gps 例外：住在 gps_nmea.c）─── */
pthread_mutex_t g_gps_lock = PTHREAD_MUTEX_INITIALIZER;
AccelData g_accel = {0};
pthread_mutex_t g_accel_lock = PTHREAD_MUTEX_INITIALIZER;
DataRingBuffer g_databuf;        /* 1号buffer：采集 → 处理 */
DataRingBuffer g_procbuf;        /* 2号buffer：处理 → 存储    ★B方案 */
volatile int g_running = 1;      /* ⭐所有线程的退出开关 */
DBHandle g_db;                   /* 组3：数据库句柄 */
pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;  /* 组3：写库互斥锁 */

/* Ctrl+C 信号处理：只置标志，绝不做别的 */
void sigint_handler(int sig)
{
    (void)sig;
    printf("\n收到退出信号，正在停止所有线程...\n");
    g_running = 0;
}

/* 工具函数（Day3 的老朋友，原样搬来） */
void get_timestamp(char *buf, int size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}

/* 线程函数声明（实现在各自文件） */
extern void *gps_thread(void *);
extern void *sensor_thread(void *);
extern void *collector_thread(void *);
extern void *process_thread(void *);
extern void *store_thread(void *);
/* can_thread 的声明在 thread_can.h */

int main(void)
{
    pthread_t tid[6];
    int nthreads = 0;
    int can_ok;

    signal(SIGINT, sigint_handler);

    /* 初始化两个环形buffer（各容量100→可存99条） */
    if (rb_init(&g_databuf, 100) != 0 || rb_init(&g_procbuf, 100) != 0) {
        fprintf(stderr, "buffer初始化失败\n");
        return -1;
    }

    printf("=== 车载传感器采集框架 v1.0 ===\n");
    printf("按 Ctrl+C 退出\n\n");

    if (db_open(&g_db, "/home/root/data/vehicle.db") != 0) {   /* 组3：开库 */
        fprintf(stderr, "数据库初始化失败，退出\n");
        return -1;
    }

    /* ★组4：CAN 初始化必须在 pthread_create 之前（线程里直接 read 这个 fd） */
    can_ok = (can_linux_init("can0") == 0);

    /* 创建五个原有线程 */
    pthread_create(&tid[nthreads++], NULL, gps_thread,       NULL);
    pthread_create(&tid[nthreads++], NULL, sensor_thread,    NULL);
    pthread_create(&tid[nthreads++], NULL, collector_thread, NULL);
    pthread_create(&tid[nthreads++], NULL, process_thread,   NULL);
    pthread_create(&tid[nthreads++], NULL, store_thread,     NULL);

    /* ★组4：第 6 个线程——CAN 接收（失败则降级，不影响其他线程） */
    if (can_ok) {
        pthread_create(&tid[nthreads++], NULL, can_thread,   NULL);
    } else {
        fprintf(stderr, "[CAN] 初始化失败，CAN 线程不启动（其他线程照常运行）\n");
    }

    /* 主线程等所有线程退出 */
    for (int i = 0; i < nthreads; i++)
        pthread_join(tid[i], NULL);

    /* 清理：谁 init 谁 destroy / close */
    rb_destroy(&g_databuf);
    rb_destroy(&g_procbuf);
    db_close(&g_db);
    if (can_ok)
        can_linux_close();
    printf("程序已退出\n");
    return 0;
}
