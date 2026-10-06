/* SPDX-License-Identifier: MIT */

#ifndef _I915_DP_H_
#define _I915_DP_H_

#include "igt_kms.h"

int i915_dp_get_current_link_rate(int drm_fd, igt_output_t *output);
int i915_dp_get_current_lane_count(int drm_fd, igt_output_t *output);
int i915_dp_get_max_link_rate(int drm_fd, igt_output_t *output);
int i915_dp_get_max_lane_count(int drm_fd, igt_output_t *output);
void i915_dp_force_link_retrain(int drm_fd, igt_output_t *output, int retrain_count);
void i915_dp_force_lt_failure(int drm_fd, igt_output_t *output, int failure_count);
bool i915_dp_get_link_retrain_disabled(int drm_fd, igt_output_t *output);
bool i915_dp_has_force_link_training_failure_debugfs(int drmfd, igt_output_t *output);
int i915_dp_get_pending_lt_failures(int drm_fd, igt_output_t *output);
int i915_dp_get_pending_retrain(int drm_fd, igt_output_t *output);
void i915_dp_reset_link_params(int drm_fd, igt_output_t *output);
void i915_dp_set_link_params(int drm_fd, igt_output_t *output,
			     char *link_rate, char *lane_count);
int i915_dp_get_max_supported_rate(int drm_fd, const igt_output_t *output);

#endif
