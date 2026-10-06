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

	igt_describe("Query GPU device time.");
	igt_subtest("get-time") {
		struct drm_asahi_get_time time = {};

		igt_asahi_get_time(fd, &time, 0);
		// Nothing to assert on, the timestamp could have any value
	}

	igt_describe("Query GPU device time twice and compare timestamps.");
	igt_subtest("get-time-compare") {
		struct drm_asahi_get_time time1 = {};
		struct drm_asahi_get_time time2 = {};

		igt_asahi_get_time(fd, &time1, 0);

		// sleep for 100 micro seconds to ensure
		usleep(100);

		igt_asahi_get_time(fd, &time2, 0);

		// assert that the timestamps are different
		igt_assert(time1.gpu_timestamp != time2.gpu_timestamp);
	}

	igt_describe("Query GPU device time with invalid flags values.");
	igt_subtest_group() {
		struct drm_asahi_get_time time = {};

		igt_subtest("get-time-flags-1") {
			time.flags = 1;
			igt_asahi_get_time(fd, &time, EINVAL);
		}
		igt_subtest("get-time-flags-2") {
			time.flags = 2;
			igt_asahi_get_time(fd, &time, EINVAL);
		}
		igt_subtest("get-time-flags-uint64-max") {
			time.flags = UINT64_MAX;
			igt_asahi_get_time(fd, &time, EINVAL);
		}
	}

	igt_fixture() {
		drm_close_driver(fd);
	}
}
