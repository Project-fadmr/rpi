#include "radar_parser.h"
#include "radar_ring.h"

#define PRINT_POINTS 0      // 1 = also print every detected point (slow at high point counts)

static radar_parser_t parser;
static radar_ring_t   ring;
static radar_stats_t  prev_avg;
static bool           have_prev = false;

static void print_stats(const char *label, const radar_stats_t *s) {
    Serial.print("  "); Serial.print(label);
    Serial.print(": x=");       Serial.print(s->x, 3);
    Serial.print("  y=");       Serial.print(s->y, 3);
    Serial.print("  z=");       Serial.print(s->z, 3);
    Serial.print("  doppler="); Serial.println(s->doppler, 3);
}

#if PRINT_POINTS
static void on_point(const radar_frame_t *f, uint32_t i, const radar_point_t *pt, void *user) {
    (void)user;
    Serial.print("frame "); Serial.print(f->frame_num);
    Serial.print("  point "); Serial.print(i);
    Serial.print(":  x=");    Serial.print(pt->x, 3);
    Serial.print("  y=");     Serial.print(pt->y, 3);
    Serial.print("  z=");     Serial.print(pt->z, 3);
    Serial.print("  doppler="); Serial.println(pt->doppler, 3);
}
#endif

/* Called once per complete frame, from loop() context. */
static void on_frame(const radar_frame_t *f, void *user) {
    (void)user;
    radar_stats_t avg, mean;
    if (radar_calc_frame(f, &avg) != 0) return;   /* no points: keep previous average */
    radar_calc_mean(f, &mean);

    Serial.print("frame "); Serial.print(f->frame_num);
    Serial.print(" ("); Serial.print(f->num_points); Serial.println(" points)");
    print_stats("avg |v| ", &avg);
    print_stats("mean    ", &mean);

    if (have_prev) {
        radar_stats_t d = radar_calc_delta(&prev_avg, &avg);
        print_stats("delta   ", &d);
    }
    Serial.println();
    prev_avg = avg;
    have_prev = true;
}

/* ------------------------------------------------------------------
 * THE ONLY PART LEFT FOR YOU: call this from your ESP-NOW receive
 * callback with the raw payload. It is safe to call from an ISR.
 * ------------------------------------------------------------------ */
void espnow_on_receive(const uint8_t *data, int len) {
    radar_ring_push(&ring, data, (size_t)len);
}

void setup() {
    Serial.begin(115200);
    radar_ring_init(&ring);
#if PRINT_POINTS
    radar_init(&parser, on_point, on_frame, NULL);
#else
    radar_init(&parser, NULL, on_frame, NULL);
#endif

    // TODO (yours): initialise ESP-NOW and register espnow_on_receive()
}

void loop() {
    uint8_t chunk[128];
    size_t n;
    while ((n = radar_ring_pop(&ring, chunk, sizeof chunk)) > 0) {
        radar_feed(&parser, chunk, n);
    }

    static uint32_t last_dropped = 0;
    if (ring.dropped != last_dropped) {          // overflow: stream misaligned, resync
        last_dropped = ring.dropped;
        radar_reset(&parser);
        Serial.println("warning: receive buffer overflow, resyncing");
    }
}
