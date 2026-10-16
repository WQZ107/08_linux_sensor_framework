/* src/main.c —— 程序入口：初始化共享资源、创建5个线程、优雅退出 */
#include "common.h"
#include "ringbuf.h"
#include <signal.h>

/* ─── 全局共享资源定义（common.h 里 extern 声明过；g_gps 例外：住在 gps_nmea.c）─── */
pthread_mutex_t g_gps_lock = PTHREAD_MUTEX_INITIALIZER;
AccelData g_accel = {0};
pthread_mutex_t g_accel_lock = PTHREAD_MUTEX_INITIALIZER;
DataRingBuffer g_databuf;
volatile int g_running = 1;      /* ⭐所有线程的退出开关 */

/* Ctrl+C 信号处理：只置标志，绝不做别的（第2步唯一新知识点） */
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

/* 线程函数声明（实现在各自文件，第3步逐个替换成真身） */
extern void *gps_thread(void *);
extern void *sensor_thread(void *);
extern void *collector_thread(void *);
extern void *process_thread(void *);
extern void *store_thread(void *);

/* ─── 骨架阶段的临时空线程（第3步逐个换成真活）─── */
static void *dummy(int id)
{
    while (g_running) {
        printf("[线程%d] 活着\n", id);
        sleep(1);
    }
    printf("[线程%d] 退出\n", id);
    return NULL;
}
void *gps_thread(void *a)      { (void)a; return dummy(1); }
void *sensor_thread(void *a)   { (void)a; return dummy(2); }
void *collector_thread(void *a){ (void)a; return dummy(3); }
void *process_thread(void *a)  { (void)a; return dummy(4); }
void *store_thread(void *a)    { (void)a; return dummy(5); }

int main(void)
{
    pthread_t tid[5];

    signal(SIGINT, sigint_handler);          /* 1. 注册 Ctrl+C 处理 */

    if (rb_init(&g_databuf, 100) != 0) {     /* 2. 初始化环形buffer（容量100→可存99条） */
        fprintf(stderr, "buffer初始化失败\n");
        return -1;
    }

    printf("=== 车载传感器采集框架 v1.0 ===\n");
    printf("按 Ctrl+C 退出\n\n");

    /* 3. 创建五个线程 */
    pthread_create(&tid[0], NULL, gps_thread,       NULL);
    pthread_create(&tid[1], NULL, sensor_thread,    NULL);
    pthread_create(&tid[2], NULL, collector_thread, NULL);
    pthread_create(&tid[3], NULL, process_thread,   NULL);
    pthread_create(&tid[4], NULL, store_thread,     NULL);

    /* 4. 主线程等所有线程退出（Ctrl+C 后它们陆续退） */
    for (int i = 0; i < 5; i++)
        pthread_join(tid[i], NULL);

    /* 5. 清理（谁 init 谁 destroy，和 create/free 一个道理） */
    rb_destroy(&g_databuf);
    printf("程序已退出\n");
    return 0;
}
