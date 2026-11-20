#ifndef RADAR_RING_H
#define RADAR_RING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Single-producer / single-consumer byte ring buffer.
 * Producer = your ESP-NOW receive callback (ISR or other context).
 * Consumer = loop(). Safe without locks as long as there is exactly one of each. */
#define RADAR_RING_SIZE 2048u            /* must be a power of two */

typedef struct {
    volatile uint32_t head;              /* written by producer */
    volatile uint32_t tail;              /* written by consumer */
    volatile uint32_t dropped;           /* bytes lost to overflow */
    uint8_t data[RADAR_RING_SIZE];
} radar_ring_t;

void   radar_ring_init(radar_ring_t *r);
/* Returns number of bytes stored; the rest are counted in r->dropped. */
size_t radar_ring_push(radar_ring_t *r, const uint8_t *src, size_t len);
/* Returns number of bytes copied into dst (0 if empty). */
size_t radar_ring_pop(radar_ring_t *r, uint8_t *dst, size_t max);

#ifdef __cplusplus
}
#endif
#endif
