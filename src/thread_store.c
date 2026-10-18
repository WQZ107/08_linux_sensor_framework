/* src/thread_store.c —— 存储线程 v2.0：SQLite 事务批量（组3：CSV → 数据库） */
#include "common.h"
#include "ringbuf.h"
#include "database.h"

#define BATCH_SIZE 10                     /* 攒10条一次事务落盘（约10秒一批） */

extern DBHandle g_db;                     /* main.c 定义：数据库句柄 */
extern pthread_mutex_t g_db_lock;         /* main.c 定义：写库互斥锁 */

/* 把本批用一个事务提交：10条共享一次落盘 */
static void flush_batch(DataPoint *batch, int n)
{
    pthread_mutex_lock(&g_db_lock);       /* 同一连接不许多线程同时写 */

    if (sqlite3_exec(g_db.db, "BEGIN TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "[STORAGE] BEGIN失败: %s\n", sqlite3_errmsg(g_db.db));
        pthread_mutex_unlock(&g_db_lock);
        return;
    }

    pthread_mutex_lock(&g_gps_lock);      /* 锁内只拷数据：卫星数从 g_gps 拷出 */
    int sat = g_gps.satellites;
    pthread_mutex_unlock(&g_gps_lock);

    for (int i = 0; i < n; i++) {
        db_insert(&g_db, batch[i].timestamp,
                  batch[i].lat, batch[i].lon, batch[i].speed,
                  batch[i].accel_total, batch[i].temperature,
                  batch[i].humidity, batch[i].pitch, batch[i].roll, batch[i].distance,
                  sat, batch[i].alert);
        if (batch[i].alert) {             /* 告警行额外记 alert_log（同一事务内） */
            char detail[64];
            snprintf(detail, sizeof(detail), "speed=%.0f accel=%.2f",
                     batch[i].speed, batch[i].accel_total);
            db_insert_alert(&g_db, batch[i].timestamp,
                            batch[i].speed > 100 ? "OVER_SPEED" : "HARD_ACCEL",
                            detail);
        }
    }

    sqlite3_exec(g_db.db, "COMMIT;", NULL, NULL, NULL);
    pthread_mutex_unlock(&g_db_lock);
    printf("[STORAGE] 批量入库 %d 条\n", n);              /* 约10秒一条＝心跳指纹 */
}

void *store_thread(void *arg)
{
    (void)arg;
    printf("[存储线程] 启动（SQLite事务批量，每 %d 条一提交）\n", BATCH_SIZE);

    DataPoint batch[BATCH_SIZE];
    int batch_count = 0, stored = 0;

    while (g_running)                     /* 数据源仍是 g_procbuf（B方案2号buffer），架构不动 */
    {
        DataPoint dp;
        if (rb_pop_timeout(&g_procbuf, &dp, 1) != 0)  /* 1秒超时→回while看g_running */
            continue;

        batch[batch_count++] = dp;
        if (batch_count >= BATCH_SIZE) {  /* 攒满→事务提交 */
            flush_batch(batch, batch_count);
            stored += batch_count;
            batch_count = 0;
        }
    }

    /* 退出收尾：排干buffer＋尾巴提交（教材版会丢这里的数据） */
    DataPoint dp;
    while (rb_pop_timeout(&g_procbuf, &dp, 0) == 0) {  /* 0=非阻塞，g_running=0也能取 */
        batch[batch_count++] = dp;
        if (batch_count >= BATCH_SIZE) {
            flush_batch(batch, batch_count);
            stored += batch_count;
            batch_count = 0;
        }
    }
    if (batch_count > 0) {                /* 尾巴(<10条)也走事务 */
        flush_batch(batch, batch_count);
        stored += batch_count;
    }

    printf("[存储线程] 退出，共入库 %d 条\n", stored);
    return NULL;
}
