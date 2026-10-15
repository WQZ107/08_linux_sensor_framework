#include "gps_nmea.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

GPS_Data g_gps = {0};

/* ★Day48-1:nmea_get_field(完整代码见昨天 Day48 步骤2,原样抄) */
/* 从 NMEA 语句里抽第 index 个字段(0=语句头) 到 out,返回字段长度 */
int nmea_get_field(const char *str, int index, char *out, int outlen)
{
    int i = 0, n = 0;

    /* 1) 跳过前 index 个逗号 */
    while (*str != '\0' && i < index) {
        if (*str == ',') i++;
        str++;
    }
    /* 2) 复制到 逗号 / '*' / 结尾 为止(坑:不能把 *校验和 当字段) */
    while (*str != ',' && *str != '*' && *str != '\0' && n < outlen - 1) {
        out[n++] = *str++;
    }
    out[n] = '\0';                       /* 3) 补字符串结束符 */
    return n;
}

/* ★Day48-2:nmea_to_deg(完整代码见 Day48 步骤3,原样抄) */
/* "2307.1234",2 -> 23.118723;  "11315.6789",3 -> 113.261315 */
double nmea_to_deg(const char *coord, int deg_len)
{
    char degbuf[8] = {0};
    int  deg;
    double minutes;

    strncpy(degbuf, coord, deg_len);     /* 度的部分:纬度2位/经度3位 */
    degbuf[deg_len] = '\0';
    deg     = atoi(degbuf);
    minutes = atof(coord + deg_len);     /* 分的部分:小数点后的字符串直接转浮点 */
    return deg + minutes / 60.0;
}

/* ================= Day49:解析器本体 ================= */
/* RMC:推荐最小定位信息(字段序见手抄笔记) */
static void parse_rmc(const char *line)
{
    char f[20];

    /* 状态守卫(KP-G6):状态≠'A' 直接返回——
       无效定位后面字段全空,解析它=把 g_gps 污染成 (0,0) 假坐标 */
    nmea_get_field(line, 2, f, sizeof(f));
        if (f[0] != 'A') {
        g_gps.valid = 0;
        g_gps.latitude  = 0;    /* V 时清掉旧值,显示和 RAW(空)一致 */
        g_gps.longitude = 0;
        g_gps.speed_kmh = 0;
        return;
    }
    g_gps.valid = 1;

    /* UTC 时间 hhmmss.sss → 时/分/秒。
       上次留的作业答案:数字字符减 '0' 就是数值,
       两位数 = 十位*10+个位,如 "09" → 0*10+9=9 */
    nmea_get_field(line, 1, f, sizeof(f));
    if (strlen(f) >= 6) {
        g_gps.hour = (f[0]-'0')*10 + (f[1]-'0');
        g_gps.min  = (f[2]-'0')*10 + (f[3]-'0');
        g_gps.sec  = (f[4]-'0')*10 + (f[5]-'0');
    }

    /* 纬度:字段3(deg_len=2)+ 字段4 半球,南纬为负 */
    nmea_get_field(line, 3, f, sizeof(f));
    g_gps.latitude = nmea_to_deg(f, 2);
    nmea_get_field(line, 4, f, sizeof(f));
    if (f[0] == 'S') g_gps.latitude = -g_gps.latitude;

    /* 经度:字段5(deg_len=3!纬2经3,踩坑头条)+ 字段6,西经为负 */
    nmea_get_field(line, 5, f, sizeof(f));
    g_gps.longitude = nmea_to_deg(f, 3);
    nmea_get_field(line, 6, f, sizeof(f));
    if (f[0] == 'W') g_gps.longitude = -g_gps.longitude;

    /* 速度:字段7,单位是"节",×1.852 才是 km/h */
    nmea_get_field(line, 7, f, sizeof(f));
    g_gps.speed_kmh = atof(f) * 1.852;
}

/* GGA:只取卫星数(字段7,紧随 Quality 字段6 之后) */
static void parse_gga(const char *line)
{
    char f[20];
    nmea_get_field(line, 7, f, sizeof(f));
    g_gps.satellites = (uint8_t)atoi(f);
}

/* 分发入口:一行完整 NMEA 进来,按类型分家 */
void GPS_ParseLine(const char *line)
{
    if (line[0] != '$') return;    /* 防空行/半行/脏字符 */

    /* 只比第4~6字符:GN/GP/BD/GL 什么前缀都通吃 */
    if      (strncmp(line + 3, "RMC", 3) == 0) parse_rmc(line);
    else if (strncmp(line + 3, "GGA", 3) == 0) parse_gga(line);
    /* 其余(GSV/VTG/ZDA/GLL...)直接忽略 */
}

/* ★Day48-3:TestNmea(先抄 T1~T4 四段测试,原样抄) */
void TestNmea(void)
{
    char f[20];

    /* 测试用例必须来自真模块的语句格式(手册踩坑第4条) */
    char test[] = "$GNRMC,093000.000,A,2307.1234,N,11315.6789,E,0.5,30.1,150726,,,A*60";

    nmea_get_field(test, 3, f, sizeof(f));
    printf("[T1] field3=[%s]  expect [2307.1234]\r\n", f);

    nmea_get_field(test, 9, f, sizeof(f));
    printf("[T2] field9=[%s]  expect [150726]\r\n", f);

    printf("[T3] lat: %.6f  expect 23.118723\r\n", nmea_to_deg("2307.1234", 2));
    printf("[T4] lon: %.6f  expect 113.261315\r\n", nmea_to_deg("11315.6789", 3));
    
        /* T5:V 状态必须安全跳过(验收2)——用你板上的真实空语句 */
    GPS_ParseLine("$GNRMC,,V,,,,,,,,,,N,V*37");
    printf("[T5] after V: valid=%d lat=%.6f (expect 0 / 0.000000)\r\n",
           g_gps.valid, g_gps.latitude);

    /* T6:定位语句喂进去,全字段人工对数(验收1) */
    GPS_ParseLine("$GNRMC,093000.000,A,2307.1234,N,11315.6789,E,0.5,30.1,150726,,,A*60");
    GPS_ParseLine("$GNGGA,093000.000,2307.1234,N,11315.6789,E,1,08,0.9,25.0,M,0.0,M,,*66");
    printf("[T6] valid=%d lat=%.6f lon=%.6f spd=%.1f sat=%d time=%02u:%02u:%02u\r\n",
           g_gps.valid, g_gps.latitude, g_gps.longitude,
           g_gps.speed_kmh, g_gps.satellites,
           g_gps.hour, g_gps.min, g_gps.sec);
}
