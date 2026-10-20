/* include/can_linux.h —— Linux SocketCAN 收发模块（126-140 第2组） */
#ifndef __CAN_LINUX_H__
#define __CAN_LINUX_H__

#include <stdint.h>

/* 创建套接字并绑定到指定接口（如 "can0"）；0=成功，-1=失败 */
int  can_linux_init(const char *interface);

/* 发一帧标准帧；0=成功，-1=失败 */
int  can_linux_send(uint32_t id, const uint8_t *data, uint8_t len);

/* 取套接字 fd（第4组 CAN 线程 read 用） */
int  can_linux_getfd(void);

/* 关闭套接字（测试程序收尾用） */
void can_linux_close(void);

#endif
