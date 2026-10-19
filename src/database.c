#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_open(DBHandle *h, const char *path)
{
    int rc = sqlite3_open(path, &h->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] 打开失败: %s\n", sqlite3_errmsg(h->db));
        return -1;
    }

    /* 性能两件套:WAL(写不阻塞读) + NORMAL同步级别 */
    sqlite3_exec(h->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(h->db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    /* 建表+索引(与组1命令行建的表同构;索引名idx_ts,组1里叫idx_timestamp,同一个东西两个名) */
    char *err = NULL;
    rc = sqlite3_exec(h->db,
        "CREATE TABLE IF NOT EXISTS data_point ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  latitude REAL, longitude REAL,"
        "  speed REAL, accel_total REAL,"
        "  temperature REAL, humidity REAL,"
        "  pitch REAL, roll REAL, distance REAL,"
        "  satellites INTEGER,"
        "  alert INTEGER DEFAULT 0);"
        "CREATE INDEX IF NOT EXISTS idx_ts ON data_point(timestamp);"
        "CREATE TABLE IF NOT EXISTS alert_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT NOT NULL,"
        "  alert_type TEXT NOT NULL,"
        "  detail TEXT);",
        NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] 建表失败: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    /* 预编译插入语句:12个?占位符,只编译这一次 */
    rc = sqlite3_prepare_v2(h->db,
        "INSERT INTO data_point (timestamp, latitude, longitude,"
        " speed, accel_total, temperature, humidity, pitch, roll, distance,"
        " satellites, alert)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
        -1, &h->stmt_insert, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] 预编译失败: %s\n", sqlite3_errmsg(h->db));
        return -1;
    }

    printf("[DB] 数据库初始化成功: %s\r\n", path);
    return 0;
}

int db_insert(DBHandle *h, const char *ts,
              double lat, double lon,
              float speed, float accel,
              float temp, float humi,
              float pitch, float roll, float dist,
              int sat, int alert)
{
    /* bind 序号从1开始!12个?对应1~12 */
    sqlite3_bind_text(h->stmt_insert, 1, ts, -1, SQLITE_STATIC);
    sqlite3_bind_double(h->stmt_insert, 2, lat);
    sqlite3_bind_double(h->stmt_insert, 3, lon);
    sqlite3_bind_double(h->stmt_insert, 4, speed);
    sqlite3_bind_double(h->stmt_insert, 5, accel);
    sqlite3_bind_double(h->stmt_insert, 6, temp);
    sqlite3_bind_double(h->stmt_insert, 7, humi);
    sqlite3_bind_double(h->stmt_insert, 8, pitch);
    sqlite3_bind_double(h->stmt_insert, 9, roll);
    sqlite3_bind_double(h->stmt_insert, 10, dist);
    sqlite3_bind_int(h->stmt_insert, 11, sat);
    sqlite3_bind_int(h->stmt_insert, 12, alert);

    int rc = sqlite3_step(h->stmt_insert);   /* 执行 */
    sqlite3_reset(h->stmt_insert);           /* 复位,下次重新bind */

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[DB] 插入失败: %s\n", sqlite3_errmsg(h->db));
        return -1;
    }
    return 0;
}

/* 低频写入用exec拼SQL(值全是程序内部常量,无外部输入,允许拼) */
int db_insert_alert(DBHandle *h, const char *ts,
                    const char *type, const char *detail)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO alert_log (timestamp, alert_type, detail) "
             "VALUES ('%s', '%s', '%s');", ts, type, detail);
    char *err = NULL;
    if (sqlite3_exec(h->db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "[DB] 告警插入失败: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* 查询写法A:exec+回调(每查到一行自动调query_cb一次) */
static int query_cb(void *data, int argc, char **argv, char **colname)
{
    /* SELECT * 返回13列: id=0,timestamp=1,lat=2,lon=3,speed=4,...
       alert=argv[12]; argv里全是字符串,要数值自己atof/atoi */
    printf("%s | lat=%.6f spd=%.0f %s\r\n",
           argv[1], atof(argv[2]), atof(argv[4]),
           atoi(argv[12]) ? "!" : "");
    return 0;   /* 返回0继续查下一行 */
}

int db_query_range(DBHandle *h, const char *start, const char *end)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT * FROM data_point WHERE timestamp BETWEEN '%s' AND '%s';",
             start, end);
    char *err = NULL;
    if (sqlite3_exec(h->db, sql, query_cb, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "[DB] 查询失败: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* 查询写法B:prepare/step+column(聚合统计,一行结果) */
int db_stats(DBHandle *h)
{
    sqlite3_stmt *stmt;
    int count = 0;
    double max_speed = 0, avg_speed = 0;

    if (sqlite3_prepare_v2(h->db,
        "SELECT COUNT(*), MAX(speed), AVG(speed) FROM data_point;",
        -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] 统计prepare失败: %s\n", sqlite3_errmsg(h->db));
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {   /* 有结果行 */
        count     = sqlite3_column_int(stmt, 0);      /* column列号从0数! */
        max_speed = sqlite3_column_double(stmt, 1);
        avg_speed = sqlite3_column_double(stmt, 2);
    }
    sqlite3_finalize(stmt);

    printf("[统计] 总:%d 最高:%.0f 平均:%.1f\r\n", count, max_speed, avg_speed);
    return 0;
}

/* 按时间段导出CSV(组4):prepare/step 遍历 + column 逐列取值
   ★列号对应12列表 SELECT 的12列:0=timestamp 1=lat 2=lon 3=speed 4=accel
    5=temp 6=humi 7=pitch 8=roll 9=dist 10=sat 11=alert */
int db_export_csv(DBHandle *h, const char *start, const char *end,
                  const char *csvfile)
{
    FILE *fp = fopen(csvfile, "w");
    if (!fp) { perror("创建CSV失败"); return -1; }

    /* 表头(12列,与组1表结构一致;带BOM防Excel中文乱码) */
    fprintf(fp, "\xEF\xBB\xBF时间,纬度,经度,速度,加速度,温度,湿度,俯仰,横滚,距离,卫星,告警\n");

    /* 外部输入的时间范围:严格应绑?;内部工具+固定格式,教材允许拼接(见组4边界) */
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT timestamp, latitude, longitude, speed, accel_total,"
             " temperature, humidity, pitch, roll, distance, satellites, alert"
             " FROM data_point WHERE timestamp BETWEEN '%s' AND '%s'"
             " ORDER BY timestamp;",
             start, end);

    sqlite3_stmt *stmt;
    int rows = 0;
    if (sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {      /* 取到没有下一行为止 */
            fprintf(fp, "%s,%.6f,%.6f,%.0f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d\r\n",
                    sqlite3_column_text(stmt, 0),
                    sqlite3_column_double(stmt, 1),
                    sqlite3_column_double(stmt, 2),
                    sqlite3_column_double(stmt, 3),
                    sqlite3_column_double(stmt, 4),
                    sqlite3_column_double(stmt, 5),
                    sqlite3_column_double(stmt, 6),
                    sqlite3_column_double(stmt, 7),
                    sqlite3_column_double(stmt, 8),
                    sqlite3_column_double(stmt, 9),
                    sqlite3_column_int(stmt, 10),
                    sqlite3_column_int(stmt, 11));
            rows++;
        }
        sqlite3_finalize(stmt);                          /* 每个prepare配一个finalize */
    } else {
        fprintf(stderr, "[DB] 导出prepare失败: %s\r\n", sqlite3_errmsg(h->db));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    printf("[导出] %d条 → %s\r\n", rows, csvfile);
    return rows;
}

void db_close(DBHandle *h)
{
    sqlite3_finalize(h->stmt_insert);   /* 每个prepare要有且只有一个finalize */
    sqlite3_close(h->db);
    printf("[DB] 数据库已关闭\r\n");
}
