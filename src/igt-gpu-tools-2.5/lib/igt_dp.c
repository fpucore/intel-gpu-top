// SPDX-License-Identifier: MIT
/*
 * Copyright © 2026 Google
 *
 * Authors:
 *   Louis Chauvet <louis.chauvet@bootlin.com>
 */

#include <stdint.h>

#include "drmtest.h"
#include "i915/i915_dp.h"
#include "igt_core.h"
#include "igt_kms.h"
#include "igt_dp.h"

/**
 * igt_dp_get_current_link_rate: Get current link rate on a display port
 * @drm_fd: DRM file descriptor
 * @output: igt_output_t object representing the display port
 *
 * Returns:
 * The current link rate in kb/s, or a negative error code on failure.
 */
int igt_dp_get_current_link_rate(int drm_fd, igt_output_t *output)
{
	if (is_intel_device(drm_fd))
		/*
		 * i915_dp_get_current_link_rate returns the value in tens of kb/s because
		 * that what the kernel uses. Convert it to kb/s to have a sane unit...
		 */
		return i915_dp_get_current_link_rate(drm_fd, output) * 10;

	igt_assert_f(false, "Current drm device is not able to report used link rate\n");
	return -EINVAL;
}

/**
 * igt_dp_get_current_lane_count: Get current lane count on a display port
 * @drm_fd: DRM file descriptor
 * @output: igt_output_t object representing the display port
 *
 * Returns:
 * The number of active lanes, or a negative error code on failure.
 */
int igt_dp_get_current_lane_count(int drm_fd, igt_output_t *output)
{
	if (is_intel_device(drm_fd))
		return i915_dp_get_current_lane_count(drm_fd, output);

	igt_assert_f(false, "Current drm device is not able to report used lane count\n");
	return -EINVAL;
}

/**
 * igt_dp_get_max_link_rate: Get maximum link rate on a display port
 * @drm_fd:		DRM file descriptor
 * @output:		igt_output_t object representing the display port
 *
 * Returns:
 * The maximum link rate in kb/s, or a negative error code on failure.
 */
int igt_dp_get_max_link_rate(int drm_fd, igt_output_t *output)
{
	if (is_intel_device(drm_fd))
		/*
		 * i915_dp_get_max_link_rate returns the value in tens of kb/s because
		 * that what the kernel uses. Convert it to kb/s to have a sane unit...
		 */
		return i915_dp_get_max_link_rate(drm_fd, output) * 10;

	igt_assert_f(false, "Current drm device is not able to report max link rate\n");
	return -EINVAL;
}

/**
 * igt_dp_get_max_supported_rate: Get maximum supported link rate on a display port
 * @drm_fd: DRM file descriptor
 * @output: igt_output_t object representing the display port
 *
 * Returns:
 * The maximum supported link rate in kb/s, or a negative error code on failure.
 */
int igt_dp_get_max_supported_rate(int drm_fd, igt_output_t *output)
{
	if (is_intel_device(drm_fd))
		/*
		 * i915_dp_get_max_supported_rate returns the value in tens of kb/s because
		 * that what the kernel uses. Convert it to kb/s to have a sane unit...
		 */
		return i915_dp_get_max_supported_rate(drm_fd, output) * 10;

	igt_assert_f(false, "Current drm device is not able to report max link rate\n");
	return -EINVAL;
}

/**
 * igt_dp_get_max_lane_count: Get maximum lane count on a display port
 * @drm_fd: DRM file descriptor
 * @output: igt_output_t object representing the display port
 *
 * Returns:
 * The maximum number of lanes, or a negative error code on failure.
 */
int igt_dp_get_max_lane_count(int drm_fd, igt_output_t *output)
{
	if (is_intel_device(drm_fd))
		return i915_dp_get_max_lane_count(drm_fd, output);

	igt_assert_f(false, "Current drm device is not able to report max lane count\n");
	return -EINVAL;
}

/**
 * igt_dp_force_link_retrain: Force link retraining on a display port
 * @drm_fd:		DRM file descriptor
 * @output:		igt_output_t object representing the display port
 * @retrain_count:	Number of times to retrain the link
 */
void igt_dp_force_link_retrain(int drm_fd, igt_output_t *output, int retrain_count)
{
	if (is_intel_device(drm_fd)) {
		i915_dp_force_link_retrain(drm_fd, output, retrain_count);
		return;
	}

	igt_assert_f(false, "Current drm device does not support link retraining\n");
}

/**
 * igt_dp_get_pending_retrain: Get pending link retrain count
 * @drm_fd:		DRM file descriptor
 * @output:		igt_output_t object representing the display port
 *
 * Returns:
 * The number of pending retrain operations, or a negative error code on failure.
 */
int igt_dp_get_pending_retrain(int drm_fd, igt_output_t *output)
{
	if (is_intel_device(drm_fd))
		return i915_dp_get_pending_retrain(drm_fd, output);

	igt_assert_f(false, "Current drm device does not support pending retrain count checking\n");
	return -EINVAL;
}

/**
 * igt_dp_wait_pending_retrain: Wait for pending link retrain operations to complete
 * @drm_fd:		DRM file descriptor
 * @output:		igt_output_t object representing the display port
 *
 * This function waits for any pending link retrain operations to complete on the
 * specified display port. It polls the debugfs interface for the pending retrain
 * count until it reaches zero, indicating all retrain operations have completed.
 */
void igt_dp_wait_pending_retrain(int drm_fd, igt_output_t *output)
{
	double timeout = igt_default_display_detect_timeout();
	struct timespec start, now;

	igt_assert_eq(igt_gettime(&start), 0);

	while (1) {
		if (!igt_dp_get_pending_retrain(drm_fd, output))
			return;

		igt_assert_eq(igt_gettime(&now), 0);

		if (igt_time_elapsed(&start, &now) >= timeout)
			break;

		usleep(10000);
	}
	igt_assert_f(false, "Timeout waiting for pending retrain to complete\n");
}
