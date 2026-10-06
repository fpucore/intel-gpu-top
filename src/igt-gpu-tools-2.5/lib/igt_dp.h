/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2026 Google
 *
 * Authors:
 *   Louis Chauvet <louis.chauvet@bootlin.com>
 */

#ifndef _IGT_DP_H_
#define _IGT_DP_H_

#include "igt_kms.h"

int igt_dp_get_current_link_rate(int drm_fd, igt_output_t *output);

int igt_dp_get_current_lane_count(int drm_fd, igt_output_t *output);

int igt_dp_get_max_link_rate(int drm_fd, igt_output_t *output);

int igt_dp_get_max_supported_rate(int drm_fd, igt_output_t *output);

int igt_dp_get_max_lane_count(int drm_fd, igt_output_t *output);

void igt_dp_force_link_retrain(int drm_fd, igt_output_t *output, int retrain_count);

int igt_dp_get_pending_retrain(int drm_fd, igt_output_t *output);

void igt_dp_wait_pending_retrain(int drm_fd, igt_output_t *output);

#endif
