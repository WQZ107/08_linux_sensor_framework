#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "serial.h"

int serial_open(const char *device, int baudrate)
{
    int fd = open(device, O_RDWR | O_NOCTTY);   /* 打开设备文件:串口=文件 */
    if (fd < 0) {
        perror("open serial");
        return -1;
    }

    struct termios opt;
    tcgetattr(fd, &opt);                        /* ① 取当前配置 */

    speed_t speed;
    switch (baudrate) {
        case 9600:   speed = B9600;   break;
        case 115200: speed = B115200; break;
        default:     speed = B9600;   break;
    }
    cfsetispeed(&opt, speed);                   /* ② 波特率(收/发) */
    cfsetospeed(&opt, speed);

    opt.c_cflag &= ~PARENB;                     /* ③ 8N1:无校验 */
    opt.c_cflag &= ~CSTOPB;                     /*    1停止位 */
    opt.c_cflag &= ~CSIZE;
    opt.c_cflag |= CS8;                         /*    8数据位 */
    opt.c_cflag |= (CLOCAL | CREAD);            /*    使能接收 */

    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* ④ 原始模式:收到的就是原始字节 */
    opt.c_iflag &= ~(IXON | IXOFF | IXANY);
    opt.c_oflag &= ~OPOST;

    opt.c_cc[VMIN]  = 0;                        /* ⑤ 读超时:最多等1秒就返回 */
    opt.c_cc[VTIME] = 10;

    tcsetattr(fd, TCSANOW, &opt);               /* 应用配置 */
    return fd;
}
