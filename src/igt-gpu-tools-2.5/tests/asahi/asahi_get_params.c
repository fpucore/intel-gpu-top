// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (C) Asahi Linux contributors

#include "igt.h"
#include "igt_core.h"
#include "igt_asahi.h"
#include "asahi_drm.h"
#include <stdint.h>

int igt_main()
{
	int fd;

	igt_fixture() {
		fd = drm_open_driver_render(DRIVER_ASAHI);
	}

	igt_describe("Query global GPU parameters from device.");
	igt_subtest("get-params") {
		struct drm_asahi_params_global globals = {};

		igt_asahi_get_params(fd, 0, &globals, sizeof(globals), 0);

		// Supported GPU generations start with G13G
		igt_assert(globals.gpu_generation >= 13);
		// chip id is expected to be non zero
		igt_assert(globals.chip_id != 0);
		// VM should contain some space
		igt_assert(globals.vm_end > globals.vm_start);
		// the driver is expected to request some space for the
		// kernel in a VM
		igt_assert(globals.vm_kernel_min_size > 0);
		// the frequency of the clock used to generate timestamps
		igt_assert(globals.command_timestamp_frequency_hz > 0);
	}

	igt_describe("Query global GPU parameters for invalid param_groups.");
	igt_subtest_group() {
		struct drm_asahi_params_global globals = {};

		igt_subtest("get-params-1") {
			igt_asahi_get_params(fd, 1, &globals, sizeof(globals), EINVAL);
		}
		igt_subtest("get-params-2") {
			igt_asahi_get_params(fd, 2, &globals, sizeof(globals), EINVAL);
		}
		igt_subtest("get-params-uint32-max") {
			igt_asahi_get_params(fd, UINT32_MAX, &globals, sizeof(globals), EINVAL);
		}
	}

	igt_fixture() {
		drm_close_driver(fd);
	}
}
