/* src/can_parser.c —— 按协议表把 can_frame 拆成物理量（与 STM32 发送端严格镜像） */
#include "can_parser.h"
#include <string.h>

/* int16 大端：高字节在前，转成有符号 */
static int16_t be16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

int can_parse_frame(const struct can_frame *frame, CanData *out)
{
    uint32_t id;

    if (frame == NULL || out == NULL) return -1;
    id = frame->can_id & 0x7FF;    /* 取低 11 位：去掉 EFF/RTR 标志 */
    out->event_id = 0;             /* 默认不是事件帧 */

    switch (id) {
    case 0x100: memcpy(&out->lat, frame->data, 4); return 0;  /* float 小端：原样拷 */
    case 0x101: memcpy(&out->lon, frame->data, 4); return 0;
    case 0x102: out->speed    = be16(&frame->data[0]); return 0;
    case 0x103: out->temp_x10 = be16(&frame->data[0]); return 0;
    case 0x108: out->humi_x10 = be16(&frame->data[0]); return 0;
    case 0x109: out->pitch10  = be16(&frame->data[0]);          /* 一帧两值 */
                out->roll10   = be16(&frame->data[2]); return 0;
    case 0x10A: out->dist_mm  = be16(&frame->data[0]); return 0;
    case 0x104: out->ax100    = be16(&frame->data[0]); return 0;
    case 0x105: out->ay100    = be16(&frame->data[0]); return 0;
    case 0x106: out->az100    = be16(&frame->data[0]); return 0;
    case 0x107: out->sat        = frame->data[0];
                out->alarm_flag = frame->data[1]; return 0;
    case 0x300: out->event_id = 0x300; out->event_val = be16(&frame->data[0]); return 0;
    case 0x301: out->event_id = 0x301;
                out->event_p10 = be16(&frame->data[0]);
                out->event_r10 = be16(&frame->data[2]); return 0;
    case 0x302: out->event_id = 0x302; out->event_val = be16(&frame->data[0]); return 0;
    default:    return -1;         /* 未知 ID：丢弃 */
    }
}
