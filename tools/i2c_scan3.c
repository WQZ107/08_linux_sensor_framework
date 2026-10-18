/* tools/i2c_scan3.c —— 纯 I2C 探针（该内核不吃 SMBus 仿真;2026-10-17 实测）
   用法: ./i2c_scan3 /dev/i2c-1
   原理: 对每个地址 先"快写0字节"再"读1字节",任一步 ACK 即认为器件在线 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "用法: %s /dev/i2c-X\n", argv[0]); return 1; }
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    printf("扫描v3 %s (纯I2C探针):\n", argv[1]);
    for (int a = 0x03; a <= 0x77; a++) {
        if (ioctl(fd, I2C_SLAVE_FORCE, a) < 0) continue;
        char b, c = 0;
        int hit = 0;
        if (write(fd, &c, 0) == 0)          hit = 1;   /* 快写(0字节)ACK */
        else if (read(fd, &b, 1) == 1)      hit = 2;   /* 读回1字节 */
        if (hit) printf("  发现: 0x%02x  (%s)\n", a, hit == 1 ? "快写ACK" : "读回1字节");
    }
    close(fd);
    return 0;
}
