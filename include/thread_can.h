/* include/thread_can.h —— CAN 接收线程与共享区接口（126-140 第4组） */
#ifndef __THREAD_CAN_H__
#define __THREAD_CAN_H__

#include "can_parser.h"

/* 线程入口：pthread_create 用 */
void *can_thread(void *arg);

/* 取 CAN 共享区的最新一份（锁内整份拷贝） */
void CAN_GetData(CanData *out);

#endif
