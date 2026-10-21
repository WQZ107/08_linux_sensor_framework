/* src/can_test.c —— 第2/3组测试程序（独立 main，不进框架 OBJS）
 * 用法：./can_test [收帧秒数，默认10]
 * 流程：解析器自检 → 初始化 can0 → 发 0x200/01 → 收帧并解码打印
 */
#include "can_linux.h"
#include "can_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <linux/can.h>

/* ---------- 解析器自检：不依赖总线，先验协议表 ---------- */
static void selftest(void)
{
    struct can_frame f;
    CanData d;
    int ok = 0, total = 0;
    float lat = 23.123456f;

    memset(&d, 0, sizeof(d));

#define MK(id, b0,b1,b2,b3, dlc) do { \
        memset(&f, 0, sizeof(f)); f.can_id = (id); f.can_dlc = (dlc); \
        f.data[0]=(b0); f.data[1]=(b1); f.data[2]=(b2); f.data[3]=(b3); \
    } while (0)
#define CHECK(name, cond) do { total++; \
        if (cond) { ok++; printf("[SELFTEST] %-8s OK\n", name); } \
        else      { printf("[SELFTEST] %-8s FAIL\n", name); } } while (0)

    MK(0x100, 0,0,0,0, 4);
    memcpy(f.data, &lat, 4);
    can_parse_frame(&f, &d);
    CHECK("0x100", d.lat > lat - 0.0001f && d.lat < lat + 0.0001f);

    MK(0x101, 0xCB,0x85,0xE2,0x42, 4);
    can_parse_frame(&f, &d);
    CHECK("0x101", d.lon > 113.2f && d.lon < 113.3f);

    MK(0x102, 0x00,0x46,0,0, 2);
    can_parse_frame(&f, &d);
    CHECK("0x102", d.speed == 70);

    MK(0x103, 0x01,0x22,0,0, 2);        /* 290 = 29.0C */
    can_parse_frame(&f, &d);
    CHECK("0x103", d.temp_x10 == 290);

    MK(0x108, 0x02,0x1C,0,0, 2);        /* 540 = 54.0% */
    can_parse_frame(&f, &d);
    CHECK("0x108", d.humi_x10 == 540);

    MK(0x109, 0xFF,0xCE,0x00,0x2D, 4);  /* -50 / +45 */
    can_parse_frame(&f, &d);
    CHECK("0x109", d.pitch10 == -50 && d.roll10 == 45);

    MK(0x10A, 0x02,0x55,0,0, 2);        /* 597 mm */
    can_parse_frame(&f, &d);
    CHECK("0x10A", d.dist_mm == 597);

    MK(0x107, 0x08,0x01,0,0, 2);
    can_parse_frame(&f, &d);
    CHECK("0x107", d.sat == 8 && d.alarm_flag == 1);

    MK(0x300, 0x01,0x1A,0,0, 2);        /* 282 = 2.82g */
    can_parse_frame(&f, &d);
    CHECK("0x300", d.event_id == 0x300 && d.event_val == 282);

    MK(0x302, 0x00,0x17,0,0, 2);        /* 23 mm */
    can_parse_frame(&f, &d);
    CHECK("0x302", d.event_id == 0x302 && d.event_val == 23);

    MK(0x123, 0,0,0,0, 1);
    CHECK("unknown", can_parse_frame(&f, &d) == -1);

#undef MK
#undef CHECK
    printf("[SELFTEST] %d/%d passed\n", ok, total);
}

/* ---------- 实帧解码打印 ---------- */
static void pr_x10(const char *label, int v)
{
    printf("%s=%s%d.%d", label, (v < 0) ? "-" : "", abs(v) / 10, abs(v) % 10);
}

static void show_parsed(const struct can_frame *f, const CanData *d)
{
    uint32_t id = f->can_id & 0x7FF;

    switch (id) {
    case 0x100: printf("   -> lat=%.6f\n", d->lat); break;
    case 0x101: printf("   -> lon=%.6f\n", d->lon); break;
    case 0x102: printf("   -> speed=%d km/h\n", d->speed); break;
    case 0x103: printf("   -> "); pr_x10("temp", d->temp_x10); printf(" C\n"); break;
    case 0x108: printf("   -> "); pr_x10("humi", d->humi_x10); printf(" %%RH\n"); break;
    case 0x109: printf("   -> "); pr_x10("P", d->pitch10);
                printf(" "); pr_x10("R", d->roll10); printf("\n"); break;
    case 0x10A: printf("   -> dist=%d mm\n", d->dist_mm); break;
    case 0x107: printf("   -> sat=%d flag=%d\n", d->sat, d->alarm_flag); break;
    case 0x300: printf("   [EVENT] 急刹 acc=%.2f g\n", d->event_val / 100.0); break;
    case 0x301: printf("   [EVENT] 侧翻 "); pr_x10("P", d->event_p10);
                printf(" "); pr_x10("R", d->event_r10); printf("\n"); break;
    case 0x302: printf("   [EVENT] 防撞 dist=%d mm\n", d->event_val); break;
    default: break;                     /* 0x104~106 只打原始帧，不刷屏 */
    }
}

int main(int argc, char *argv[])
{
    int seconds = (argc > 1) ? atoi(argv[1]) : 10;
    int count = 0;
    CanData cd;

    memset(&cd, 0, sizeof(cd));
    selftest();                          /* 先验协议表，再看总线 */

    if (can_linux_init("can0") != 0) {
        fprintf(stderr, "初始化失败：can0 在吗？up 了吗？\n");
        return 1;
    }

    uint8_t cmd = 0x01;
    if (can_linux_send(0x200, &cmd, 1) == 0)
        printf("[TX] 已发送 0x200 [1] 01（请求数据）\n");

    printf("[RX] 开始收帧 %d 秒...\n", seconds);
    time_t start = time(NULL);

    while (time(NULL) - start < seconds) {
        struct can_frame frame;
        int n = read(can_linux_getfd(), &frame, sizeof(frame));

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("[RX] read");
            break;
        }
        if (n != sizeof(frame)) continue;

        uint32_t id = frame.can_id & 0x7FF;
        printf("[RX] ID=0x%03X DLC=%d Data:", id, frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++) printf(" %02X", frame.data[i]);
        printf("\n");

        if (can_parse_frame(&frame, &cd) == 0)
            show_parsed(&frame, &cd);

        count++;
    }

    printf("[DONE] 共收到 %d 帧\n", count);
    can_linux_close();
    return 0;
}
