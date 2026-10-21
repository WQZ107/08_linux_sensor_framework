/* include/can_parser.h —— CAN 帧 → 物理量（126-140 第3组） */
#ifndef __CAN_PARSER_H__
#define __CAN_PARSER_H__

#include <stdint.h>
#include <linux/can.h>

/* 持久化的“最新 CAN 数据”：解析器只改本帧对应的字段，其余保持原值。
   第4组把它当 CAN 共享区，加锁后直接交给解析器写。 */
typedef struct {
    float lat, lon;              /* 0x100/0x101  float 小端 */
    int   speed;                 /* 0x102  km/h */
    int   temp_x10;              /* 0x103  温度 ×10 */
    int   humi_x10;              /* 0x108  湿度 ×10 */
    int   pitch10, roll10;       /* 0x109  姿态 ×10 */
    int   dist_mm;               /* 0x10A  距离 mm */
    int   ax100, ay100, az100;   /* 0x104/105/106  加速度 0.01g */
    int   sat;                   /* 0x107 data[0] 卫星数 */
    int   alarm_flag;            /* 0x107 data[1] 告警标志 */
    uint32_t event_id;           /* 0x300/301/302 事件ID；0=普通数据帧 */
    int   event_val;             /* 0x300 合加速度×100 / 0x302 距离mm */
    int   event_p10, event_r10;  /* 0x301 侧翻 pitch/roll ×10 */
} CanData;

/* 解析一帧：只更新本帧对应字段；0=成功，-1=未知ID */
int can_parse_frame(const struct can_frame *frame, CanData *out);

#endif
