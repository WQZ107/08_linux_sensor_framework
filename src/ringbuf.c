/* src/ringbuf.c —— Day4 骨架 + 锁 + 条件变量 +（第4步）带超时的 pop */
#include "common.h"
#include "ringbuf.h"

int rb_init(DataRingBuffer *rb, int size)
{
    rb->buffer = malloc(size * sizeof(DataPoint));
    if (rb->buffer == NULL) return -1;
    rb->head = 0;  rb->tail = 0;  rb->size = size;
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
    pthread_cond_init(&rb->not_full, NULL);
    return 0;
}

/* 生产者：满了就睡，等消费者取走后叫醒 */
void rb_push(DataRingBuffer *rb, DataPoint *data)
{
    pthread_mutex_lock(&rb->lock);
    while ((rb->tail + 1) % rb->size == rb->head)     /* 满（⚠️必须while） */
        pthread_cond_wait(&rb->not_full, &rb->lock);  /* 睡觉等空位 */
    rb->buffer[rb->tail] = *data;
    rb->tail = (rb->tail + 1) % rb->size;
    pthread_mutex_unlock(&rb->lock);
    pthread_cond_signal(&rb->not_empty);              /* 叫醒消费者 */
}

/* 消费者：空了就睡，等生产者放入后叫醒 */
void rb_pop(DataRingBuffer *rb, DataPoint *data)
{
    pthread_mutex_lock(&rb->lock);
    while (rb->head == rb->tail)                      /* 空（⚠️必须while） */
        pthread_cond_wait(&rb->not_empty, &rb->lock);
    *data = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % rb->size;
    pthread_mutex_unlock(&rb->lock);
    pthread_cond_signal(&rb->not_full);               /* 叫醒生产者 */
}

void rb_destroy(DataRingBuffer *rb)
{
    free(rb->buffer);
    pthread_mutex_destroy(&rb->lock);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
}

/* ★第4步：带超时的 pop —— 最多睡 timeout_sec 就自己醒，回来检查 g_running
   返回 0=取到数据；-1=超时没数据 */
int rb_pop_timeout(DataRingBuffer *rb, DataPoint *data, int timeout_sec)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_sec;                       /* 绝对截止时间 = 现在 + timeout */

    pthread_mutex_lock(&rb->lock);
    while (rb->head == rb->tail && g_running)       /* ⭐还是 while（虚假唤醒铁律不变） */
    {
        if (pthread_cond_timedwait(&rb->not_empty, &rb->lock, &ts) != 0)
            break;                                  /* 超时返回非0 → 跳出去 */
    }
    if (rb->head == rb->tail) {                     /* 超时了还没数据 */
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }
    *data = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % rb->size;
    pthread_mutex_unlock(&rb->lock);
    pthread_cond_signal(&rb->not_full);
    return 0;
}
