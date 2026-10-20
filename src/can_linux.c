/* src/can_linux.c —— Linux SocketCAN 收发模块
 * 四步曲：socket(PF_CAN,SOCK_RAW,CAN_RAW) → ioctl(SIOCGIFINDEX)
 *        → bind(sockaddr_can) → write/read(struct can_frame)
 */
#include "can_linux.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

static int can_fd = -1;

int can_linux_init(const char *interface)
{
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct timeval tv;

    can_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_fd < 0) {
        perror("[CAN] socket");
        return -1;
    }

    /* ① 接口名 → 接口索引（教材漏查返回值，这里必须查） */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    if (ioctl(can_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("[CAN] ioctl SIOCGIFINDEX");
        close(can_fd); can_fd = -1;
        return -1;
    }

    /* ② 把套接字绑到 can0 */
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(can_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[CAN] bind");
        close(can_fd); can_fd = -1;
        return -1;
    }

    /* ③ 接收超时 1 秒：没帧时 read 返回 -1/EAGAIN（第4组优雅退出的基础） */
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    setsockopt(can_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("[CAN] SocketCAN 初始化成功: %s (ifindex=%d)\n",
           interface, ifr.ifr_ifindex);
    return 0;
}

int can_linux_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    struct can_frame frame;

    if (can_fd < 0) return -1;
    if (len > 8)    return -1;

    memset(&frame, 0, sizeof(frame));   /* 清干净：别把栈垃圾发上总线 */
    frame.can_id  = id;                 /* 标准帧 11 位 ID */
    frame.can_dlc = len;                /* 0~8（内核 4.1.15 用 can_dlc） */
    memcpy(frame.data, data, len);

    if (write(can_fd, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("[CAN] write");
        return -1;
    }
    return 0;
}

int can_linux_getfd(void) { return can_fd; }

void can_linux_close(void)
{
    if (can_fd >= 0) { close(can_fd); can_fd = -1; }
}
