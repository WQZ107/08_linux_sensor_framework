/* src/mpu6050_linux.c —— MPU6050 的 Linux 版驱动（"一切皆文件"） */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>     /* I2C_SLAVE（Linaro 工具链 sysroot 自带） */
#include <stdint.h>
#include "common.h"
#include "mpu6050_linux.h"

#define I2C_DEV   "/dev/i2c-1"   /* ⚠️改成你实测扫到 0x68 的那条 */
#define MPU_ADDR  0x68           /* AD0 接地=0x68 */

static int i2c_fd = -1;

int MPU6050_Linux_Init(void)
{
    i2c_fd = open(I2C_DEV, O_RDWR);
    if (i2c_fd < 0) { perror("open i2c"); return -1; }

    if (ioctl(i2c_fd, I2C_SLAVE, MPU_ADDR) < 0) {   /* 指定本次会话的从机地址 */
        perror("ioctl I2C_SLAVE"); return -1;
    }

    uint8_t buf[2] = {0x6B, 0x00};                  /* PWR_MGMT_1 写 0：解除睡眠 */
    if (write(i2c_fd, buf, 2) != 2) { perror("wake"); return -1; }
    usleep(50000);

    printf("[MPU6050] Linux I2C 初始化成功（%s, addr=0x%02X）\n", I2C_DEV, MPU_ADDR);
    return 0;
}

int MPU6050_Linux_ReadAccel(float *ax, float *ay, float *az)
{
    uint8_t reg = 0x3B;                             /* ACCEL_XOUT_H：XYZ 各 H/L 共 6 字节 */
    uint8_t buf[6];
    if (write(i2c_fd, &reg, 1) != 1) return -1;     /* 先告诉它"我要读 0x3B 起" */
    if (read(i2c_fd, buf, 6) != 6) return -1;       /* 再连续读 6 字节 */

    int16_t rx = (buf[0] << 8) | buf[1];            /* 高<<8 | 低（Day52 老手法） */
    int16_t ry = (buf[2] << 8) | buf[3];
    int16_t rz = (buf[4] << 8) | buf[5];

    *ax = rx / 16384.0f;                            /* ±2g 量程：16384 LSB/g */
    *ay = ry / 16384.0f;
    *az = rz / 16384.0f;
    return 0;
}
