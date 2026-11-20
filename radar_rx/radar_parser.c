#include "radar_parser.h"
#include <string.h>
#include <math.h>

#define HDR_REST   32u      /* header bytes after the 8 magic bytes */
#define HDR_LEN    40u
#define MAX_FRAME  (64u * 1024u)

static const uint8_t MAGIC[8] = {2, 1, 4, 3, 6, 5, 8, 7};

enum { S_SYNC, S_HEADER, S_TLV_HEAD, S_POINTS, S_SKIP, S_PAD };

static uint32_t rd_u32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static float rd_f32(const uint8_t *b) {
    uint32_t u = rd_u32(b);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

void radar_reset(radar_parser_t *p) {
    p->state = S_SYNC;
    p->magic_idx = 0;
    p->have = 0;
}

void radar_init(radar_parser_t *p, radar_point_cb point_cb, radar_frame_cb frame_cb, void *user) {
    memset(p, 0, sizeof *p);
    p->point_cb = point_cb;
    p->frame_cb = frame_cb;
    p->user = user;
    radar_reset(p);
}

static void finish_frame(radar_parser_t *p) {
    if (p->frame_cb) p->frame_cb(&p->frame, p->user);
    radar_reset(p);
}

/* Called when a TLV (or the header) is done: go to next TLV, padding, or finish. */
static void next_tlv(radar_parser_t *p) {
    p->have = 0;
    if (p->tlvs_left > 0) {
        p->state = S_TLV_HEAD;
    } else if (p->pos < p->total) {
        p->state = S_PAD;
    } else {
        finish_frame(p);
    }
}

static void feed_byte(radar_parser_t *p, uint8_t b) {
    switch (p->state) {

    case S_SYNC:
        if (b == MAGIC[p->magic_idx]) {
            if (++p->magic_idx == sizeof MAGIC) {
                p->magic_idx = 0;
                p->have = 0;
                p->state = S_HEADER;
            }
        } else {
            p->magic_idx = (b == MAGIC[0]) ? 1 : 0;   /* magic has no repeated bytes */
        }
        break;

    case S_HEADER:
        p->buf[p->have++] = b;
        if (p->have < HDR_REST) break;
        /* version, total, platform, frameNum, timeCpuCycles, numObj, numTLVs, subFrame */
        p->total = rd_u32(p->buf + 4);
        if (p->total < HDR_LEN || (p->total % 32) || p->total > MAX_FRAME) {
            radar_reset(p);                           /* false magic */
            break;
        }
        memset(&p->frame, 0, sizeof p->frame);
        p->frame.frame_num = rd_u32(p->buf + 12);
        p->frame.time_cpu_cycles = rd_u32(p->buf + 16);
        p->tlvs_left = rd_u32(p->buf + 24);
        p->pos = HDR_LEN;
        next_tlv(p);
        break;

    case S_TLV_HEAD:
        p->buf[p->have++] = b;
        if (p->have < 8) break;
        {
            uint32_t len = rd_u32(p->buf + 4);
            p->tlv_type = rd_u32(p->buf);
            if (p->pos + 8 + len > p->total) {        /* TLV overruns frame */
                radar_reset(p);
                break;
            }
            p->pos += 8 + len;
            p->tlvs_left--;
            p->body_left = len;
            p->have = 0;
            if (len == 0) { next_tlv(p); break; }
            p->state = (p->tlv_type == 1) ? S_POINTS : S_SKIP;
        }
        break;

    case S_POINTS:
        p->buf[p->have++] = b;
        if (p->have == 16) {
            radar_point_t pt = { rd_f32(p->buf), rd_f32(p->buf + 4),
                                 rd_f32(p->buf + 8), rd_f32(p->buf + 12) };
            p->frame.sum_x += pt.x;  p->frame.sum_y += pt.y;
            p->frame.sum_z += pt.z;  p->frame.sum_doppler += pt.doppler;
            p->frame.abs_x += fabsf(pt.x);  p->frame.abs_y += fabsf(pt.y);
            p->frame.abs_z += fabsf(pt.z);  p->frame.abs_doppler += fabsf(pt.doppler);
            if (p->point_cb) p->point_cb(&p->frame, p->frame.num_points, &pt, p->user);
            p->frame.num_points++;
            p->have = 0;
        }
        if (--p->body_left == 0) next_tlv(p);
        break;

    case S_SKIP:
        if (--p->body_left == 0) next_tlv(p);
        break;

    case S_PAD:
        if (++p->have >= p->total - p->pos) {
            p->pos = p->total;
            finish_frame(p);
        }
        break;
    }
}

void radar_feed(radar_parser_t *p, const uint8_t *data, size_t len) {
    while (len--) feed_byte(p, *data++);
}


int radar_calc_frame(const radar_frame_t *f, radar_stats_t *out) {
    if (f->num_points == 0) return -1;
    float n = (float)f->num_points;
    out->x = f->abs_x / n;  out->y = f->abs_y / n;
    out->z = f->abs_z / n;  out->doppler = f->abs_doppler / n;
    return 0;
}

int radar_calc_mean(const radar_frame_t *f, radar_stats_t *out) {
    if (f->num_points == 0) return -1;
    float n = (float)f->num_points;
    out->x = f->sum_x / n;  out->y = f->sum_y / n;
    out->z = f->sum_z / n;  out->doppler = f->sum_doppler / n;
    return 0;
}

radar_stats_t radar_calc_delta(const radar_stats_t *prev, const radar_stats_t *cur) {
    radar_stats_t d = {
        fabsf(cur->x - prev->x), fabsf(cur->y - prev->y),
        fabsf(cur->z - prev->z), fabsf(cur->doppler - prev->doppler)
    };
    return d;
}
