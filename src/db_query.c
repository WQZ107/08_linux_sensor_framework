/* src/db_query.c —— 独立查库工具(组4):不启动采集,单独查 vehicle.db
   套路 = argc/argv 解析 → strcmp 分发子命令 → 查询 → 打印(my_head 同款) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "database.h"        /* 组4甲方案:export 分支调 db_export_csv */

#define DB_PATH "/home/root/data/vehicle.db"

/* 回调A:data_point 的 SELECT *(13列,告警=argv[12]) */
static int print_dp_row(void *data, int argc, char **argv, char **colname)
{
    printf("%s | %.6f,%.6f | %.0fkm/h | %.2fg | %.1fC | %s\n",
           argv[1], atof(argv[2]), atof(argv[3]),
           atof(argv[4]), atof(argv[5]), atof(argv[6]),
           atoi(argv[12]) ? "告警" : "正常");
    return 0;
}

/* 回调B:stats 聚合结果(10列,argv[0]=COUNT...) */
static int print_stat_row(void *data, int argc, char **argv, char **colname)
{
    printf("总条数:%s 最高速度:%s 平均速度:%s\n", argv[0], argv[1], argv[2]);
    printf("最大加速度:%s 最高温度:%s 最高湿度:%s\n", argv[3], argv[4], argv[5]);
    printf("俯仰峰值:%s 横滚峰值:%s 距离最短:%s 最远:%s\n",
           argv[6], argv[7], argv[8], argv[9]);
    return 0;
}

/* 回调C:alert_log 的 SELECT *(4列,原版argv[8]越界→拆专用) */
static int print_alert_row(void *data, int argc, char **argv, char **colname)
{
    printf("%s | %-10s | %s\n", argv[1], argv[2], argv[3]);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("用法:\n");
        printf("  %s stats                 -- 统计\n", argv[0]);
        printf("  %s range 起始时间 结束时间 -- 按时间查询\n", argv[0]);
        printf("  %s alerts                -- 最近20条告警\n", argv[0]);
        printf("  %s export 起 结束 文件.csv -- 导出CSV\n", argv[0]);
        return 1;
    }

    sqlite3 *db;
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "打开库失败: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    char *err = NULL;
    char sql[512];

    if (strcmp(argv[1], "stats") == 0) {
        strcpy(sql, "SELECT COUNT(*), MAX(speed), AVG(speed), MAX(accel_total),"
                    " MAX(temperature), MAX(humidity), MAX(pitch), MAX(roll),"
                    " MIN(distance), MAX(distance) FROM data_point;");
        sqlite3_exec(db, sql, print_stat_row, NULL, &err);
    }
    else if (strcmp(argv[1], "range") == 0 && argc == 4) {
        snprintf(sql, sizeof(sql),
                 "SELECT * FROM data_point WHERE timestamp BETWEEN '%s' AND '%s';",
                 argv[2], argv[3]);
        sqlite3_exec(db, sql, print_dp_row, NULL, &err);
    }
    else if (strcmp(argv[1], "alerts") == 0) {
        strcpy(sql, "SELECT * FROM alert_log ORDER BY timestamp DESC LIMIT 20;");
        sqlite3_exec(db, sql, print_alert_row, NULL, &err);
    }
	else if (strcmp(argv[1], "export") == 0 && argc == 5) {
		int n = db_export_csv(&(DBHandle){.db = db}, argv[2], argv[3], argv[4]);
		if (n >= 0) printf("导出 %d 条到 %s\n",n, argv[4]);
	}
    else {
        fprintf(stderr, "未知命令或参数不对,用法见 %s(无参数)\n", argv[0]);
        sqlite3_close(db);
        return 1;
    }

    if (err) { fprintf(stderr, "查询错误: %s\n", err); sqlite3_free(err); }
    sqlite3_close(db);
    return 0;
}
