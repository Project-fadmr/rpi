#ifndef RADAR_PARSER_H
#define RADAR_PARSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Info about one frame. */
typedef struct {
    uint32_t frame_num;
    uint32_t time_cpu_cycles;
    uint32_t num_points;          /* points seen so far (final value in on_frame) */
    float sum_x, sum_y, sum_z, sum_doppler;           /* plain sums (for the mean)        */
    float abs_x, abs_y, abs_z, abs_doppler;           /* sums of absolute values (average) */
} radar_frame_t;

/* Four values: used for the abs-average, the signed mean and the delta. */
typedef struct { float x, y, z, doppler; } radar_stats_t;

/* One detected point. */
typedef struct { float x, y, z, doppler; } radar_point_t;

/* Called for every detected point as it is decoded. index counts from 0 within the frame. */
typedef void (*radar_point_cb)(const radar_frame_t *frame, uint32_t index,
                               const radar_point_t *pt, void *user);
/* Called once when a complete frame has been received. */
typedef void (*radar_frame_cb)(const radar_frame_t *frame, void *user);

typedef struct {
    radar_point_cb point_cb;
    radar_frame_cb frame_cb;
    void *user;

    uint8_t  state;
    uint8_t  magic_idx;
    uint8_t  have;
    uint8_t  buf[32];

    uint32_t total, pos;          /* frame length, bytes consumed so far */
    uint32_t tlvs_left;
    uint32_t tlv_type, body_left;
    radar_frame_t frame;
} radar_parser_t;

void radar_init(radar_parser_t *p, radar_point_cb point_cb, radar_frame_cb frame_cb, void *user);
void radar_reset(radar_parser_t *p);

/* Feed any amount of received bytes (e.g. one ESP-NOW payload). */
void radar_feed(radar_parser_t *p, const uint8_t *data, size_t len);

/* Average of the absolute values of the points (calc_frame in the Python version).
 * Returns 0 on success, -1 if the frame has no points. */
int radar_calc_frame(const radar_frame_t *f, radar_stats_t *out);

/* Signed mean of the points (positive and negative values cancel). Same return codes. */
int radar_calc_mean(const radar_frame_t *f, radar_stats_t *out);

/* Absolute change between two stats (calc_delta_speed in the Python version). */
radar_stats_t radar_calc_delta(const radar_stats_t *prev, const radar_stats_t *cur);

#ifdef __cplusplus
}
#endif
#endif
