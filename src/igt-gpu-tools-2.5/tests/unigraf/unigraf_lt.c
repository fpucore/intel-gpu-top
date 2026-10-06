// SPDX-License-Identifier: MIT
/*
 * Copyright © 2026 Google
 *
 * Authors:
 *   Louis Chauvet <louis.chauvet@bootlin.com>
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drm_fourcc.h"
#include "drmtest.h"
#include "igt_aux.h"
#include "igt_core.h"
#include "igt_dp.h"
#include "igt_edid.h"
#include "igt_kms.h"
#include "unigraf/unigraf.h"

/**
 * TEST: unigraf link training
 * Category: Core
 * Description: Testing DP link training with a unigraf device
 *
 * SUBTEST: unigraf-dp-lane-count
 * Description: Make sure that the link training goes well for different lane count
 *
 * SUBTEST: unigraf-dp-link-rate
 * Description: Make sure that the link training goes well for different link rates
 */

static void init_output_and_display_pattern(igt_display_t *display, igt_output_t *output)
{
	igt_crtc_t *crtc;
	struct igt_fb fb;
	igt_plane_t *primary;
	drmModeModeInfo *mode;
	int fb_id;

	igt_modeset_disable_all_outputs(display);
	igt_display_reset(display);

	igt_output_set_crtc(output, 0);
	crtc = igt_get_crtc_for_output(display, output);
	igt_output_set_crtc(output, crtc);

	/* Get the current mode */
	mode = igt_output_get_mode(output);
	igt_assert(mode);

	/* Create a framebuffer with a solid color pattern */
	fb_id = igt_create_color_pattern_fb(display->drm_fd, mode->hdisplay,
					    mode->vdisplay, DRM_FORMAT_XRGB8888,
					    DRM_FORMAT_MOD_LINEAR, 0, 0, 0, &fb);
	igt_assert(fb_id > 0);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	igt_assert(primary);

	igt_plane_set_size(primary, mode->hdisplay, mode->vdisplay);
	igt_plane_set_fb(primary, &fb);
	igt_output_override_mode(output, mode);

	igt_display_commit2(display, COMMIT_ATOMIC);

	/* Set the framebuffer to the plane */
	igt_plane_set_fb(primary, &fb);

	/* Set the plane properties atomically */
	igt_plane_set_prop_value(primary, IGT_PLANE_FB_ID, fb.fb_id);
	igt_plane_set_prop_value(primary, IGT_PLANE_CRTC_X, 0);
	igt_plane_set_prop_value(primary, IGT_PLANE_CRTC_Y, 0);
	igt_plane_set_prop_value(primary, IGT_PLANE_CRTC_W, mode->hdisplay);
	igt_plane_set_prop_value(primary, IGT_PLANE_CRTC_H, mode->vdisplay);

	/* Commit the changes atomically */
	igt_display_commit_atomic(display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
}

IGT_TEST_DESCRIPTION("Test unigraf device functionality");
int igt_main()
{
	int drm_fd = -1;
	igt_display_t display;
	igt_output_t *output;
	drmModeConnectorPtr connector;
	int i;

	igt_fixture() {
		drm_fd = drm_open_driver(DRIVER_ANY);
		igt_assert(drm_fd >= 0);

		igt_display_require(&display, drm_fd);
		unigraf_require_device(drm_fd);

		connector = unigraf_get_connector(drm_fd);
	}

	igt_subtest_with_dynamic("unigraf-dp-lane-count") {
		int lane_counts[3] = {1, 2, 4};
		int current_lanes;

		for (i = 0; i < ARRAY_SIZE(lane_counts); i++) {
			igt_dynamic_f("unigraf-dp-lane-count-%d", lane_counts[i]) {
				unigraf_reset();
				unigraf_set_max_lane_count(lane_counts[i]);
				unigraf_hpd_pulse(500000);

				igt_display_require_output(&display);
				output = igt_output_from_connector(&display, connector);
				igt_assert(output);
				igt_dp_force_link_retrain(drm_fd, output, 2);
				init_output_and_display_pattern(&display, output);

				current_lanes = igt_dp_get_current_lane_count(drm_fd, output);
				igt_assert_eq(current_lanes, lane_counts[i]);
			}
		}
	}

	igt_subtest_with_dynamic("unigraf-dp-link-rate") {
		int rates[] = {UNIGRAF_RATE_1_62_GHZ,
			       UNIGRAF_RATE_2_7_GHZ,
			       UNIGRAF_RATE_5_4_GHZ,
			       UNIGRAF_RATE_6_75_GHZ,
			       UNIGRAF_RATE_8_10_GHZ};
		int current_rate;
		int max_supported_rate;

		for (i = 0; i < ARRAY_SIZE(rates); i++) {
			igt_dynamic_f("unigraf-dp-link-rate-%d", rates[i]) {
				unigraf_reset();
				unigraf_set_max_link_rate(rates[i]);
				unigraf_hpd_pulse(1000000);
				igt_display_require_output(&display);
				igt_display_reset(&display);
				igt_display_require_output(&display);
				output = igt_output_from_connector(&display, connector);
				igt_assert(output);
				igt_dp_force_link_retrain(drm_fd, output, 2);

				init_output_and_display_pattern(&display, output);

				current_rate = igt_dp_get_max_link_rate(drm_fd, output);
				max_supported_rate = igt_dp_get_max_supported_rate(drm_fd, output);
				igt_require(max_supported_rate >= unigraf_rate_to_kbs(rates[i]));
				igt_assert_eq(current_rate, unigraf_rate_to_kbs(rates[i]));
			}
		}
	}

	igt_subtest("unigraf-dp-link-suspend-resume") {
		int current_rate, current_lanes;

		unigraf_reset();
		igt_display_require_output(&display);
		igt_display_reset(&display);
		igt_display_require_output(&display);
		output = igt_output_from_connector(&display, connector);
		igt_assert(output);

		/* Get initial link parameters */
		current_rate = igt_dp_get_current_link_rate(drm_fd, output);
		current_lanes = igt_dp_get_current_lane_count(drm_fd, output);

		/* Suspend the system */
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM, SUSPEND_TEST_NONE);

		/* Verify link parameters are maintained */
		igt_assert_eq(igt_dp_get_current_link_rate(drm_fd, output), current_rate);
		igt_assert_eq(igt_dp_get_current_lane_count(drm_fd, output), current_lanes);
	}

	igt_fixture() {
		igt_display_fini(&display);
		close(drm_fd);
	}
}
