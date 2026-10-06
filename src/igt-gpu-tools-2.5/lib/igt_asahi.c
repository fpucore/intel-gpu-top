// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (C) 2025 Collabora Ltd.
// SPDX-FileCopyrightText: Copyright (C) 2025 Asahi Linux contributors
/*
 * Based on igt_panthor.c
 */

#include "drmtest.h"
#include "igt_asahi.h"
#include "ioctl_wrappers.h"
#include "asahi_drm.h"

#include <stdint.h>

/**
 * SECTION:igt_asahi
 * @short_description: asahi support library
 * @title: Asahi
 * @include: igt.h
 *
 * This Library provides auxiliary helper functions for writing asahi tests.
 */

/**
 * igt_asahi_get_params:
 * @fd: device file descriptor
 * @param_group: which params to query parameters for
 * @params: pointer to the struct to store the parameters in
 * @size: size of the params buffer
 * @err: expected error code, 0 for success
 */
void igt_asahi_get_params(int fd, uint32_t param_group, void *params, size_t size, int err)
{
	struct drm_asahi_get_params get_params = {
		.param_group = param_group,
		.pointer = (uintptr_t)params,
		.size = size,
	};

	if (err)
		do_ioctl_err(fd, DRM_IOCTL_ASAHI_GET_PARAMS, &get_params, err);
	else
		do_ioctl(fd, DRM_IOCTL_ASAHI_GET_PARAMS, &get_params);
}

/**
 * igt_asahi_get_time:
 * @fd: device file descriptor
 * @get_time: pointer to drm_asahi_get_time struct
 * @err: expected error code, 0 for success
 */
void igt_asahi_get_time(int fd, struct drm_asahi_get_time *get_time, int err)
{
	if (err)
		do_ioctl_err(fd, DRM_IOCTL_ASAHI_GET_TIME, get_time, err);
	else
		do_ioctl(fd, DRM_IOCTL_ASAHI_GET_TIME, get_time);
}
