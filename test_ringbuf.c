/* test_ringbuf.c —— 第1步完成标志：最小验证（临时测试，不进 Makefile） */
#include <stdio.h>
#include "common.h"
#include "ringbuf.h"

static DataRingBuffer rb;

/* 生产者：每 100ms 放一条，值递增（借 lat 字段当"序号"用） */
void *producer(void *arg)
{
    (void)arg;
    int v = 1;
    while (1) {
        DataPoint dp;
        memset(&dp, 0, sizeof(dp));
        dp.lat = v;
        printf("[生产] push 第 %d 条\n", v);
        rb_push(&rb, &dp);
        v++;
        usleep(100000);
    }
    return NULL;
}

int main(void)
{
    pthread_t tid;

    printf("[测试] 开始：生产者每100ms放一条，主线程pop 5条\n");
    if (rb_init(&rb, 10) != 0) {          /* 容量10→实际存9条（判满留一格，Day4同款） */
        fprintf(stderr, "rb_init 失败\n");
        return -1;
    }
    pthread_create(&tid, NULL, producer, NULL);

    int expect = 1, bad = 0;
    for (int i = 0; i < 5; i++) {
        DataPoint dp;
        rb_pop(&rb, &dp);                  /* 空就睡，生产出来才醒 */
        int got = (int)dp.lat;
        printf("[主线程] pop: got=%d expect=%d %s\n",
               got, expect, got == expect ? "OK" : "!!不对");
        if (got != expect) bad = 1;
        expect++;
    }

    if (bad) {
        printf("== 测试失败：不递增或有重/丢 ==\n");
        return 1;
    }
    printf("== 测试通过：严格递增、不重不丢 ==\n");
    return 0;
}
