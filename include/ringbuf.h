/* include/ringbuf.h —— 线程安全环形 buffer 的接口 */
#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include "common.h"

int  rb_init(DataRingBuffer *rb, int size);
void rb_push(DataRingBuffer *rb, DataPoint *data);   /* 生产者：满了会睡 */
void rb_pop(DataRingBuffer *rb, DataPoint *data);    /* 消费者：空了会睡 */
int  rb_pop_timeout(DataRingBuffer *rb, DataPoint *data, int timeout_sec);   /* ★第4步：带超时的pop；0=取到,-1=超时 */
void rb_destroy(DataRingBuffer *rb);

#endif
