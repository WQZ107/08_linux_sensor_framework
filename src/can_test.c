/* src/can_test.c —— 第2组测试用独立小程序（⚠️不进框架 OBJS，有自己的 main）
 * 用法：./can_test [收帧秒数，默认20]
 * 流程：初始化 can0 → 发一帧 0x200/01 请求数据 → 循环收帧打印 → 到点收工
 */
#include "can_linux.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <linux/can.h>

int main(int argc, char *argv[])
{
    int seconds = (argc > 1) ? atoi(argv[1]) : 20;
    int count = 0;

    if (can_linux_init("can0") != 0) {
        fprintf(stderr, "初始化失败：can0 在吗？up 了吗？（ip link set can0 up）\n");
        return 1;
    }

    /* ① 发送测试：0x200/01 = 请求数据，STM32 会回 4 帧 */
    uint8_t cmd = 0x01;
    if (can_linux_send(0x200, &cmd, 1) == 0)
        printf("[TX] 已发送 0x200 [1] 01（请求数据）\n");
    else
        printf("[TX] 发送失败\n");

    /* ② 接收测试：边收边打印 */
    printf("[RX] 开始收帧 %d 秒...\n", seconds);
    time_t start = time(NULL);

    while (time(NULL) - start < seconds) {
        struct can_frame frame;
        int n = read(can_linux_getfd(), &frame, sizeof(frame));

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;  /* 1 秒超时，正常 */
            perror("[RX] read");
            break;
        }
        if (n != sizeof(frame)) continue;                          /* 半帧，丢弃 */

        printf("[RX] ID=0x%03X DLC=%d Data:", frame.can_id & 0x7FF, frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
            printf(" %02X", frame.data[i]);
        printf("\n");
        count++;
    }

    printf("[DONE] 共收到 %d 帧\n", count);
    can_linux_close();
    return 0;
}
