/* src/thread_store.c —— 存储线程（第3步⑤：第二个消费者——CSV 追加 + 每条 fflush 落盘） */
#include <stdio.h>     /* fopen / fprintf / fflush / fclose */
#include <unistd.h>    /* access / F_OK / fsync */
#include <sys/stat.h>  /* mkdir */
#include <errno.h>     /* strerror */
#include <string.h>    /* memset */
#include <stdlib.h>    /* NULL */

#include "common.h"
#include "ringbuf.h"   /* rb_pop 原型；g_databuf / DataPoint 来自 common.h */

#define LOG_FILE "/home/root/data/sensor_data.csv"   /* 板上持久目录 */

/* 逐级创建路径，效果等同 mkdir -p。成功返回 0 */
static int ensure_parent_dir(const char *path)
{
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "%s", path);
    if (len <= 0 || (size_t)len >= sizeof(buf))
        return -1;                                   /* 路径过长 */

    for (char *p = buf + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(buf, 0755) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    return 0;
}

void *store_thread(void *arg)
{
    (void)arg;
    printf("[存储线程] 启动，日志: %s\n", LOG_FILE);

    /* ⭐开机先建目录，省得回车就退出 */
    if (ensure_parent_dir(LOG_FILE) != 0) {
        fprintf(stderr, "[存储线程] 创建目录失败: %s\n", strerror(errno));
        return NULL;
    }

    int need_header = (access(LOG_FILE, F_OK) != 0);  /* 文件不存在才写表头 */
    FILE *fp = fopen(LOG_FILE, "a");                  /* 追加模式（Day8 是 "w" 覆盖） */
    if (fp == NULL) {
        fprintf(stderr, "[存储线程] 打开 %s 失败: %s（磁盘满？SD 卡没插？）\n",
                LOG_FILE, strerror(errno));
        return NULL;
    }

    if (need_header) {
        fputs("\xEF\xBB\xBF", fp);                   /* UTF-8 BOM，Excel 打开中文不乱码 */
        fprintf(fp, "时间,纬度,经度,速度,加速度,温度,湿度,俯仰,横滚,距离,valid,告警\n");
        fflush(fp);
    }

    int stored = 0, failed = 0;
    while (g_running)
    {
        DataPoint dp;
        if (rb_pop_timeout(&g_procbuf, &dp, 1) != 0)  /* ★第4步：带超时等；超时回 while 看 g_running */
            continue;

        int n = fprintf(fp, "%s,%.6f,%.6f,%.0f,%.2f,%.1f,%.1f,%.1f,%.1f,%.0f,%d,%d\n",
                        dp.timestamp, dp.lat, dp.lon, dp.speed,
                        dp.accel_total, dp.temperature, dp.humidity,
                        dp.pitch, dp.roll, dp.distance, dp.valid, dp.alert);
        if (n < 0) {
            /* ⭐磁盘满 / SD 卡拔出时 fprintf 静默失败，不检查就白干 */
            failed++;
            fprintf(stderr, "[存储线程] 写入失败: %s（累计失败 %d）\n",
                    strerror(errno), failed);
            if (failed >= 10) {
                fprintf(stderr, "[存储线程] 连续失败 10 次，放弃记录\n");
                break;
            }
        } else {
            fflush(fp);                               /* 数据从用户态推到内核缓存 */
            /* 真要扛断电，把下一行打开（代价见文末说明）：
             * fsync(fileno(fp));                       ← 数据真正落到存储介质 */
            stored++;
            if (stored % 5 == 0)
                printf("[STORAGE] 已存储 %d 条\n", stored);
        }
    }

    fclose(fp);
    printf("[存储线程] 退出，成功 %d 条，失败 %d 条\n", stored, failed);
    return NULL;
}
