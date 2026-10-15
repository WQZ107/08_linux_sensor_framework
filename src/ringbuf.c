/* src/ringbuf.c —— Day4 骨架 + 锁 + 条件变量 */
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
