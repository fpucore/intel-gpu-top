#include "igt.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "xf86drm.h"
#include <xf86drmMode.h>

#include "i915/gem_create.h"

/**
 * TEST: prime udl
 * Category: Display
 * Description: Basic set of prime tests between Intel and DisplayLink.
 * Driver requirement: i915
 * Mega feature: General Display Features
 * Sub-category: Memory management tests
 * Functionality: prime import/export
 * Test category: GEM_Legacy
 * Feature: prime
 *
 * SUBTEST: share-import
 *
 * SUBTEST: share-import-addfb
 *
 */

IGT_TEST_DESCRIPTION("Basic set of prime tests between Intel and DisplayLink");

int intel_fd = -1, udl_fd = -1;

#define BO_SIZE (640*480*2)

static int find_and_open_devices(void)
{
	int i;
	char path[80];
	struct stat buf;
	FILE *fl;
	char vendor_id[8];
	int venid;
	for (i = 0; i < 9 && intel_fd == -1 && udl_fd == -1; i++) {
		sprintf(path, "/sys/class/drm/card%d/device/vendor", i);
		if (stat(path, &buf)) {
			/* look for usb dev */
			sprintf(path, "/sys/class/drm/card%d/device/idVendor", i);
			if (stat(path, &buf))
				continue;
		}

		fl = fopen(path, "r");
		if (!fl)
			break;

		igt_assert(fgets(vendor_id, 8, fl) != NULL);
		fclose(fl);

		venid = strtoul(vendor_id, NULL, 16);
		sprintf(path, "/dev/dri/card%d", i);
		if (venid == 0x8086 && intel_fd == -1) {
			intel_fd = open(path, O_RDWR);
			if (intel_fd == -1)
				return -1;

			if (!is_i915_device(intel_fd)) {
				close(intel_fd);
				intel_fd = -1;
			}
		} else if (venid == 0x17e9 && udl_fd == -1) {
			udl_fd = open(path, O_RDWR);
			if (udl_fd == -1)
				return -1;
		}
	}
	return 0;
}

static int dumb_bo_destroy(int fd, uint32_t handle)
{

	struct drm_mode_destroy_dumb arg;
	int ret;
	memset(&arg, 0, sizeof(arg));
	arg.handle = handle;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
	if (ret)
		return -errno;
	return 0;

}

/*
 * simple share and import
 */
static void test1(void)
{
	uint32_t intel_handle, udl_handle;
	int prime_fd;

	intel_handle = gem_create(intel_fd, BO_SIZE);

	prime_fd = prime_handle_to_fd(intel_fd, intel_handle);
	igt_assert(prime_fd >= 0);

	udl_handle = prime_fd_to_handle(udl_fd, prime_fd);
	igt_assert(udl_handle > 0);

	dumb_bo_destroy(udl_fd, udl_handle);
	gem_close(intel_fd, intel_handle);
}

static void test2(void)
{
	uint32_t intel_handle, udl_handle;
	uint32_t fb_id;
	drmModeClip clip;
	int prime_fd;
	int ret;

	intel_handle = gem_create(intel_fd, BO_SIZE);

	prime_fd = prime_handle_to_fd(intel_fd, intel_handle);
	igt_assert(prime_fd >= 0);

	udl_handle = prime_fd_to_handle(udl_fd, prime_fd);

	ret = drmModeAddFB(udl_fd, 640, 480, 16, 16, 640, udl_handle, &fb_id);
	igt_assert(ret == 0);

	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = 10;
	clip.y2 = 10;
	ret = drmModeDirtyFB(udl_fd, fb_id, &clip, 1);
	igt_assert(ret == 0);

	dumb_bo_destroy(udl_fd, udl_handle);
	gem_close(intel_fd, intel_handle);
}

int igt_main()
{
	igt_fixture() {
		igt_assert(find_and_open_devices() >= 0);
		igt_skip_on(udl_fd == -1);
		igt_skip_on(intel_fd == -1);
	}

	/* create an object on the i915 */
	igt_subtest("share-import")
		test1();

	igt_subtest("share-import-addfb")
		test2();

	igt_fixture() {
		close(intel_fd);
		close(udl_fd);
	}
}
