/* Self-check for espnow_frame.h. Run by APKBUILD check(); on a glibc host:
 * gcc -fsanitize=address,undefined -I. frame_test.c && ./a.out */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "espnow_frame.h"

/* test-only frame builder: what an ESP sends, plus a radiotap header */
#define ESPNOW_RT_LEN		12
#define ESPNOW_TX_OVERHEAD	(ESPNOW_RT_LEN + ESPNOW_HDR_LEN + \
				 ESPNOW_BODY_HDR + ESPNOW_ELEM_HDR)

/*
 * Writes radiotap + v1.0 ESP-NOW action frame into out, which must hold
 * ESPNOW_TX_OVERHEAD + len bytes; len must be <= ESPNOW_V1_MAX.
 * Returns bytes written.
 */
static unsigned int espnow_build(u8 *out, const u8 *dst, const u8 *src,
					u16 seq, const u8 *rnd,
					const u8 *data, unsigned int len)
{
	u8 *p = out;

	/*
	 * radiotap v0, it_len 12, it_present = RATE | TX_FLAGS: the header
	 * aireplay-ng injects with; keeps the 802.11 header 4-byte aligned.
	 */
	*p++ = 0; *p++ = 0; *p++ = ESPNOW_RT_LEN; *p++ = 0;
	*p++ = 0x04; *p++ = 0x80; *p++ = 0; *p++ = 0;
	*p++ = 2;			/* 1 Mbps, in 500 kbps units */
	*p++ = 0;			/* pad: tx flags are 2-byte aligned */
	*p++ = 0x18; *p++ = 0;		/* NO_ACK | NO_SEQ, as aireplay-ng */

	*p++ = 0xd0; *p++ = 0;		/* action frame, ToDS = FromDS = 0 */
	*p++ = 0; *p++ = 0;		/* duration */
	memcpy(p, dst, 6); p += 6;
	memcpy(p, src, 6); p += 6;
	memcpy(p, espnow_bcast, 6); p += 6;
	*p++ = (seq << 4) & 0xff;	/* sequence control, little endian */
	*p++ = (seq >> 4) & 0xff;

	*p++ = 127;			/* vendor-specific action category */
	memcpy(p, espnow_oui, 3); p += 3;
	memcpy(p, rnd, 4); p += 4;

	*p++ = 221;			/* vendor-specific element */
	*p++ = 5 + len;			/* OUI + type + version + body */
	memcpy(p, espnow_oui, 3); p += 3;
	*p++ = 4;			/* type: ESP-NOW */
	*p++ = 1;			/* version 1.0 */
	memcpy(p, data, len);
	return p + len - out;
}


static const u8 me[6] = { 0x02, 0, 0, 0, 0, 1 };
static const u8 peer[6] = { 0x24, 0x6f, 0x28, 1, 2, 3 };
static const u8 rnd[4] = { 1, 2, 3, 4 };

int main(void)
{
	u8 buf[4096], out[ESPNOW_V2_MAX];
	u8 data[ESPNOW_V1_MAX];
	unsigned int n, i, j;

	for (i = 0; i < sizeof(data); i++)
		data[i] = i;

	/* round trip, unicast to us, max v1 payload, with a trailing FCS */
	n = espnow_build(buf, me, peer, 7, rnd, data, ESPNOW_V1_MAX);
	assert(n == ESPNOW_TX_OVERHEAD + ESPNOW_V1_MAX);
	assert(buf[ESPNOW_RT_LEN + 22] == 0x70);	/* seq 7 << 4 */
	assert(espnow_parse(buf + ESPNOW_RT_LEN, n - ESPNOW_RT_LEN + 4, me, NULL) == ESPNOW_V1_MAX);
	assert(espnow_parse(buf + ESPNOW_RT_LEN, n - ESPNOW_RT_LEN, me, out) == ESPNOW_V1_MAX);
	assert(!memcmp(out, data, ESPNOW_V1_MAX));

	/* empty payload, broadcast */
	n = espnow_build(buf, espnow_bcast, peer, 0, rnd, data, 0);
	assert(espnow_parse(buf + ESPNOW_RT_LEN, n - ESPNOW_RT_LEN, me, out) == 0);

	/* addressed to someone else, protected, wrong category */
	n = espnow_build(buf, peer, me, 0, rnd, data, 10);
	assert(espnow_parse(buf + ESPNOW_RT_LEN, n - ESPNOW_RT_LEN, me, out) == -1);
	n = espnow_build(buf, me, peer, 0, rnd, data, 10);
	buf[ESPNOW_RT_LEN + 1] = 0x40;
	assert(espnow_parse(buf + ESPNOW_RT_LEN, n - ESPNOW_RT_LEN, me, out) == -1);
	buf[ESPNOW_RT_LEN + 1] = 0;
	buf[ESPNOW_RT_LEN + 24] = 4;
	assert(espnow_parse(buf + ESPNOW_RT_LEN, n - ESPNOW_RT_LEN, me, out) == -1);

	/* every truncation of a valid frame is rejected or shorter, never overread */
	n = espnow_build(buf, me, peer, 0, rnd, data, 100);
	for (i = 0; i < n - ESPNOW_RT_LEN; i++)
		assert(espnow_parse(buf + ESPNOW_RT_LEN, i, me, out) == -1);

	/* v2.0: chain of six full elements = 1470 bytes; a seventh overflows */
	{
		u8 *f = buf + ESPNOW_RT_LEN, *p;
		n = espnow_build(buf, me, peer, 0, rnd, data, 0) - ESPNOW_RT_LEN - ESPNOW_ELEM_HDR;
		for (j = 0; j < 7; j++) {
			unsigned int body = j < 5 ? 250 : 220;
			p = f + n;
			p[0] = 221; p[1] = 5 + body;
			memcpy(p + 2, espnow_oui, 3); p[5] = 4;
			p[6] = 0x12;	/* v2, more data */
			memset(p + 7, j, body);
			n += ESPNOW_ELEM_HDR + body;
			if (j == 5) {
				p[6] = 0x02;	/* last element */
				assert(espnow_parse(f, n, me, out) == ESPNOW_V2_MAX);
				assert(out[0] == 0 && out[1469] == 5 && out[1250] == 5 && out[1249] == 4);
				p[6] = 0x12;
			}
		}
		assert(espnow_parse(f, n, me, out) == -1);
	}

	/* random garbage after a valid header must never crash */
	srand(1);
	for (i = 0; i < 200000; i++) {
		unsigned int len = ESPNOW_HDR_LEN + ESPNOW_BODY_HDR + rand() % 600;
		n = espnow_build(buf, me, peer, 0, rnd, data, 0);
		for (j = ESPNOW_RT_LEN + ESPNOW_HDR_LEN + ESPNOW_BODY_HDR; j < ESPNOW_RT_LEN + len; j++)
			buf[j] = (j & 3) == 0 ? 221 : rand();
		int r = espnow_parse(buf + ESPNOW_RT_LEN, len, me, out);
		assert(r >= -1 && r <= ESPNOW_V2_MAX);
	}

	puts("frame_test: ok");
	return 0;
}
