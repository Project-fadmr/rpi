#include "radar_ring.h"

#define MASK (RADAR_RING_SIZE - 1u)

void radar_ring_init(radar_ring_t *r) {
    r->head = r->tail = r->dropped = 0;
}

size_t radar_ring_push(radar_ring_t *r, const uint8_t *src, size_t len) {
    uint32_t head = r->head;
    uint32_t free_space = RADAR_RING_SIZE - (head - r->tail);
    size_t n = len < free_space ? len : free_space;
    for (size_t i = 0; i < n; i++) r->data[(head + i) & MASK] = src[i];
    __sync_synchronize();                /* data visible before head moves */
    r->head = head + (uint32_t)n;
    if (n < len) r->dropped += (uint32_t)(len - n);
    return n;
}

size_t radar_ring_pop(radar_ring_t *r, uint8_t *dst, size_t max) {
    uint32_t tail = r->tail;
    uint32_t avail = r->head - tail;
    size_t n = avail < max ? avail : max;
    for (size_t i = 0; i < n; i++) dst[i] = r->data[(tail + i) & MASK];
    __sync_synchronize();
    r->tail = tail + (uint32_t)n;
    return n;
}
