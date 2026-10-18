#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <sqlite3.h>

typedef struct {
    sqlite3 *db;                 /* 数据库句柄 */
    sqlite3_stmt *stmt_insert;   /* 预编译的插入语句(复用) */
} DBHandle;

int  db_open(DBHandle *h, const char *path);
int  db_insert(DBHandle *h, const char *ts,
               double lat, double lon,
               float speed, float accel,
               float temp, float humi,
               float pitch, float roll, float dist,
               int sat, int alert);
int  db_insert_alert(DBHandle *h, const char *ts,
                     const char *type, const char *detail);
int  db_query_range(DBHandle *h, const char *start, const char *end);
int  db_stats(DBHandle *h);
void db_close(DBHandle *h);

#endif
