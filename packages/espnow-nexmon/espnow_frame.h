/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ESP-NOW frame parser. Pure byte handling, shared by espnow.c (kernel) and
 * frame_test.c (userspace). Format: see docs/espnow.md.
 */
#ifndef ESPNOW_FRAME_H
#define ESPNOW_FRAME_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/string.h>
#else
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
#endif

#define ESPNOW_ETHERTYPE	0x88B5	/* IEEE local experimental */
#define ESPNOW_V1_MAX		250
#define ESPNOW_V2_MAX		1470
#define ESPNOW_HDR_LEN		24	/* 802.11 management header */
#define ESPNOW_BODY_HDR		8	/* category + OUI + 4 random bytes */
#define ESPNOW_ELEM_HDR		7	/* id, len, OUI, type, version */

static const u8 espnow_oui[3] = { 0x18, 0xfe, 0x34 };
static const u8 espnow_bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * Parses an 802.11 frame (radiotap already stripped; a trailing FCS is
 * tolerated). Accepts only unprotected ESP-NOW frames addressed to own or
 * broadcast. Chained v2.0 elements are concatenated.
 * If out is NULL only the payload length is computed.
 * Returns payload length (0..ESPNOW_V2_MAX) or -1 if not for us.
 */
static inline int espnow_parse(const u8 *f, unsigned int len, const u8 *own,
			       u8 *out)
{
	const u8 *p = f + ESPNOW_HDR_LEN + ESPNOW_BODY_HDR;
	const u8 *end = f + len;
	unsigned int total = 0, elen;
	int found = 0;

	if (len < ESPNOW_HDR_LEN + ESPNOW_BODY_HDR + ESPNOW_ELEM_HDR)
		return -1;
	/* action subtype; reject ToDS/FromDS and protected (encrypted) frames */
	if (f[0] != 0xd0 || (f[1] & 0x43))
		return -1;
	if (memcmp(f + 4, own, 6) && memcmp(f + 4, espnow_bcast, 6))
		return -1;
	if (f[24] != 127 || memcmp(f + 25, espnow_oui, 3))
		return -1;

	while (end - p >= ESPNOW_ELEM_HDR) {
		elen = p[1];
		if (p[0] != 221 || elen < 5 || elen > (unsigned int)(end - p - 2) ||
		    memcmp(p + 2, espnow_oui, 3) || p[5] != 4)
			break;
		if (total + elen - 5 > ESPNOW_V2_MAX)
			return -1;
		if (out)
			memcpy(out + total, p + 7, elen - 5);
		total += elen - 5;
		found = 1;
		if (!(p[6] & 0x10))	/* v2.0 "more data" flag */
			break;
		p += 2 + elen;
	}
	return found ? (int)total : -1;
}

#endif
