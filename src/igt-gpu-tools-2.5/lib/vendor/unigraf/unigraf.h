/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2026 Google
 *
 * Authors:
 *   Louis Chauvet <louis.chauvet@bootlin.com>
 */

#ifndef UNIGRAF_H
#define UNIGRAF_H

#include <stdbool.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "igt_fb.h"

/**
 * unigraf_assert: Helper macro to assert a TSI return value and retrieve a detailed error message.
 * @result: libTSI return value to check
 *
 * This macro checks the return value of a libTSI function call. If the return value indicates an
 * error, it retrieves a detailed error message and asserts with that message.
 * If retrieving the error description fails, it asserts with a generic error message.
 */
#define unigraf_assert(result)									\
({												\
	char msg[256];										\
	TSI_RESULT __r = (result);								\
	if (__r < TSI_SUCCESS) {								\
		TSI_RESULT __r2 = TSI_MISC_GetErrorDescription(__r, msg, sizeof(msg));		\
		if (__r2 < TSI_SUCCESS)								\
			igt_assert_f(false,							\
				     "unigraf error: %d (get error description failed: %d)\n",	\
				     __r, __r2);						\
		else										\
			igt_assert_f(false, "unigraf error: %d (%s)\n", __r, msg);		\
	}											\
	(__r);											\
})

bool unigraf_open_device(int drm_fd);

void unigraf_require_device(int drm_fd);

void unigraf_reset(void);

drmModeConnectorPtr unigraf_get_connector(int drm_fd);

struct edid *unigraf_read_edid(uint32_t stream, uint32_t *edid_size);

void unigraf_write_edid(uint32_t stream, const struct edid *edid, uint32_t edid_size);

void unigraf_hpd_deassert(void);

void unigraf_hpd_pulse(int duration);

void unigraf_hpd_assert(void);

void unigraf_plug(void);

void unigraf_unplug(void);

void unigraf_set_sst(void);

void unigraf_set_mst(void);

int unigraf_get_mst_stream_count(void);

bool unigraf_set_mst_stream_count(int count);

int unigraf_get_mst_stream_max_count(void);

void unigraf_select_stream(int stream);

void unigraf_read_crc(int stream, igt_crc_t *out);

bool unigraf_use_crc(void);

int unigraf_get_connector_id_by_stream(int drm_fd, int stream_id);

void unigraf_assert_stream_timings(int stream, drmModeModeInfoPtr mode_info);

int unigraf_get_max_lane_count(void);

void unigraf_set_max_lane_count(uint32_t count);

enum unigraf_rate {
	UNIGRAF_RATE_1_62_GHZ = 6,
	UNIGRAF_RATE_2_7_GHZ = 10,
	UNIGRAF_RATE_5_4_GHZ = 20,
	UNIGRAF_RATE_6_75_GHZ = 25,
	UNIGRAF_RATE_8_10_GHZ = 30,
};

void unigraf_set_max_link_rate(int rate);

int unigraf_get_max_link_rate(void);

int unigraf_rate_to_kbs(enum unigraf_rate rate);

uint32_t unigraf_get_lt_rate(void);

uint32_t unigraf_get_lt_lane_count(void);

#endif // UNIGRAF_H
