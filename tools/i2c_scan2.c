/* i2c_scan2.c —— 扫描v2:用 I2C_SLAVE_FORCE(连内核已绑定的地址也探),读寄存器 0x00 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>        /* I2C_SMBUS_* */
#include <linux/i2c-dev.h>    /* I2C_SLAVE_FORCE / I2C_SMBUS */

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "用法: %s /dev/i2c-X\n", argv[0]); return 1; }
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    printf("扫描v2 %s (FORCE寻址 读寄存器0x00):\n", argv[1]);
    for (int a = 0x03; a <= 0x77; a++) {
        if (ioctl(fd, I2C_SLAVE_FORCE, a) < 0) continue;
        struct i2c_smbus_ioctl_data d = { I2C_SMBUS_READ, 0x00, I2C_SMBUS_BYTE, NULL };
        int r = ioctl(fd, I2C_SMBUS, &d);
        if (r >= 0) printf("  发现: 0x%02x  (读0x00=0x%02x)\n", a, r);
    }
    close(fd);
    return 0;
}
