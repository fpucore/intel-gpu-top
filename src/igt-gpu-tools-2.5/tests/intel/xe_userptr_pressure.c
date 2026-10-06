// SPDX-License-Identifier: MIT
/*
 * Copyright © 2026 Intel Corporation
 */

/**
 * TEST: Exercise userptr operations under memory pressure
 * Category: Core
 * Mega feature: General Core features
 * Sub-category: Memory management tests
 * Functionality: userptr memory pressure
 * Description: Stress-test userptr bind/rebind paths while the system is
 *              under sustained memory compaction and fragmentation pressure.
 */

#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "igt.h"
#include "lib/igt_syncobj.h"
#include "xe_drm.h"

#include "xe/xe_ioctl.h"
#include "xe/xe_query.h"

/* Test configuration */
#define EXEC_ITERATIONS		1024
#define FRAG_ALLOC_COUNT	256
#define FRAG_ALLOC_SIZE		(2 << 20)	/* 2MB each - huge page sized */
#define MAX_USERPTR_BUFFERS	32
#define USERPTR_BUFFER_SIZE	(256 << 20)	/* 256MB per buffer */
#define TARGET_RAM_FILL_PCT	70		/* Fill ~70% of available RAM */
#define PRESSURE_WARMUP_SECS	2		/* Let compaction threads settle before rebinding */
#define LOG_INTERVAL		64		/* Print progress every N iterations */
#define THP_DEFRAG_PATH		"/sys/kernel/mm/transparent_hugepage/defrag"

static pthread_barrier_t barrier;
static _Atomic(bool) stop_threads;
static char saved_thp_defrag[128];
static size_t saved_thp_defrag_len;
static bool have_saved_thp_defrag;

/**
 * trigger_compaction - Trigger kernel memory compaction via /proc
 *
 * Requires root privileges.
 */
static void trigger_compaction(void)
{
	int fd;

	fd = open("/proc/sys/vm/compact_memory", O_WRONLY);
	if (fd >= 0) {
		igt_ignore_warn(write(fd, "1", 1));
		close(fd);
	}
}

/**
 * save_thp_defrag - Save the current THP defragmentation mode
 *
 * Returns true if the active mode could be parsed and saved for restoration.
 */
static bool save_thp_defrag(void)
{
	char buf[128] = {};
	char *start, *end;
	ssize_t n;
	int fd;

	/*
	 * Read the current mode.  The sysfs file lists all options with the
	 * active one enclosed in square brackets, e.g.:
	 *   always defer defer+madvise [madvise] never
	 */
	have_saved_thp_defrag = false;
	saved_thp_defrag_len = 0;
	saved_thp_defrag[0] = '\0';

	fd = open(THP_DEFRAG_PATH, O_RDONLY);
	if (fd >= 0) {
		n = read(fd, buf, sizeof(buf) - 1);
		close(fd);
		if (n > 0) {
			start = strchr(buf, '[');
			end = strchr(buf, ']');
			if (start && end && end > start + 1) {
				saved_thp_defrag_len = end - start - 1;
				if (saved_thp_defrag_len < sizeof(saved_thp_defrag)) {
					memcpy(saved_thp_defrag, start + 1,
					       saved_thp_defrag_len);
					saved_thp_defrag[saved_thp_defrag_len] = '\0';
					have_saved_thp_defrag = true;
				}
			}
		}
	}

	return have_saved_thp_defrag;
}

/**
 * write_thp_defrag - Write a THP defragmentation mode string
 * @mode: Mode string to write
 * @len: Length of @mode
 */
static void write_thp_defrag(const char *mode, size_t len)
{
	int fd;

	fd = open(THP_DEFRAG_PATH, O_WRONLY);
	if (fd >= 0) {
		igt_ignore_warn(write(fd, mode, len));
		close(fd);
	}
}

/**
 * restore_thp_defrag - Restore the saved THP defragmentation mode
 */
static void restore_thp_defrag(void)
{
	if (!have_saved_thp_defrag)
		return;

	write_thp_defrag(saved_thp_defrag, saved_thp_defrag_len);
	have_saved_thp_defrag = false;
	saved_thp_defrag_len = 0;
	saved_thp_defrag[0] = '\0';
}

/**
 * thp_defrag_exit_handler - Restore THP defrag on process exit or signal
 * @sig: Signal that triggered the exit handler, or 0 for normal exit
 */
static void thp_defrag_exit_handler(int sig)
{
	(void)sig;
	restore_thp_defrag();
}

/**
 * drop_caches - Drop page caches to free up memory
 */
static void drop_caches(void)
{
	int fd;

	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	if (fd >= 0) {
		igt_ignore_warn(write(fd, "3", 1));
		close(fd);
	}
}

/**
 * compaction_thread - Continuously trigger memory compaction
 */
static void *compaction_thread(void *arg)
{
	(void)arg;
	pthread_barrier_wait(&barrier);

	while (!stop_threads) {
		trigger_compaction();
		usleep(50);
	}

	return NULL;
}

/**
 * fragmentation_thread - Create memory fragmentation to force kcompactd activity
 *
 * Allocates huge-page-sized blocks and frees every other one, creating the
 * fragmentation pattern that causes kcompactd to migrate pages.
 */
static void *fragmentation_thread(void *arg)
{
	void *allocs[FRAG_ALLOC_COUNT];
	int i, round = 0;

	(void)arg;

	memset(allocs, 0, sizeof(allocs));
	pthread_barrier_wait(&barrier);

	while (!stop_threads) {
		/* Allocate and touch blocks to get them into physical memory */
		for (i = 0; i < FRAG_ALLOC_COUNT && !stop_threads; i++) {
			allocs[i] = mmap(NULL, FRAG_ALLOC_SIZE, PROT_READ | PROT_WRITE,
					 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
			if (allocs[i] != MAP_FAILED) {
				/* Touch to ensure allocation */
				memset(allocs[i], round & 0xff, FRAG_ALLOC_SIZE);
				/* Try madvise HUGEPAGE to encourage THP and compaction */
				madvise(allocs[i], FRAG_ALLOC_SIZE, MADV_HUGEPAGE);
			}
		}

		/* Free every other block to fragment physical memory */
		for (i = 0; i < FRAG_ALLOC_COUNT; i += 2) {
			if (allocs[i] && allocs[i] != MAP_FAILED) {
				munmap(allocs[i], FRAG_ALLOC_SIZE);
				allocs[i] = NULL;
			}
		}

		/* Trigger compaction while fragmented - this migrates pages */
		trigger_compaction();
		usleep(1000);

		/* Free the remainder */
		for (i = 1; i < FRAG_ALLOC_COUNT; i += 2) {
			if (allocs[i] && allocs[i] != MAP_FAILED) {
				munmap(allocs[i], FRAG_ALLOC_SIZE);
				allocs[i] = NULL;
			}
		}

		round++;
	}

	/* Cleanup any remaining */
	for (i = 0; i < FRAG_ALLOC_COUNT; i++) {
		if (allocs[i] && allocs[i] != MAP_FAILED)
			munmap(allocs[i], FRAG_ALLOC_SIZE);
	}

	return NULL;
}

/**
 * test_rebind_under_compaction - Rebind userptr while physical memory is under compaction pressure
 * @fd: DRM file descriptor
 *
 * Allocates userptr buffers totaling ~TARGET_RAM_FILL_PCT% of available RAM,
 * then repeatedly rebinds one buffer while background threads continuously
 * compact and fragment physical memory.  When userptr dominates physical
 * memory the kernel is forced to migrate userptr pages during compaction,
 * creating a window where the rebind path and the compaction path compete
 * for the same locks.
 */
static void
test_rebind_under_compaction(int fd)
{
	uint64_t base_addr = 0x100000000;	/* Start at 4GB VA */
	void *userptr_buffers[MAX_USERPTR_BUFFERS];
	int num_buffers;
	struct drm_xe_sync sync = {
		.type = DRM_XE_SYNC_TYPE_SYNCOBJ,
		.flags = DRM_XE_SYNC_FLAG_SIGNAL,
	};
	uint32_t vm;
	pthread_t compaction_tid, frag_tid;
	uint64_t avail_ram_mb;
	int i, iter;

	/*
	 * Drop caches before measuring available RAM so the buffer count
	 * reflects genuinely free memory rather than whatever the system
	 * happens to have cached at test start.
	 */
	drop_caches();
	avail_ram_mb = igt_get_avail_ram_mb();

	/*
	 * Fill ~TARGET_RAM_FILL_PCT% of RAM with userptr buffers.
	 * USERPTR_BUFFER_SIZE per buffer keeps individual allocations
	 * manageable while still dominating physical memory.
	 */
	num_buffers = (avail_ram_mb * TARGET_RAM_FILL_PCT / 100) /
		      (USERPTR_BUFFER_SIZE >> 20);
	if (num_buffers > MAX_USERPTR_BUFFERS)
		num_buffers = MAX_USERPTR_BUFFERS;
	if (num_buffers < 4)
		num_buffers = 4;

	igt_info("Allocating %d userptr buffers of %zu MB each (total: %zu MB)\n",
		 num_buffers, (size_t)(USERPTR_BUFFER_SIZE >> 20),
		 (size_t)num_buffers * (USERPTR_BUFFER_SIZE >> 20));

	vm = xe_vm_create(fd, 0, 0);
	sync.handle = syncobj_create(fd, 0);

	/* Allocate, touch, and bind all buffers */
	for (i = 0; i < num_buffers; i++) {
		uint64_t addr = base_addr + (uint64_t)i * USERPTR_BUFFER_SIZE * 2;

		userptr_buffers[i] = mmap(NULL, USERPTR_BUFFER_SIZE, PROT_READ | PROT_WRITE,
					  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		igt_assert(userptr_buffers[i] != MAP_FAILED);

		/* Fault in all pages so they occupy physical memory */
		memset(userptr_buffers[i], 0xff, USERPTR_BUFFER_SIZE);

		sync.flags = DRM_XE_SYNC_FLAG_SIGNAL;
		xe_vm_bind_userptr_async(fd, vm, 0,
					 to_user_pointer(userptr_buffers[i]),
					 addr, USERPTR_BUFFER_SIZE, &sync, 1);
		igt_assert(syncobj_wait(fd, &sync.handle, 1, INT64_MAX, 0, NULL));
		syncobj_reset(fd, &sync.handle, 1);
	}

	/* Start compaction and fragmentation threads */
	stop_threads = false;
	igt_assert_eq(pthread_barrier_init(&barrier, NULL, 3), 0);	/* main + 2 threads */
	igt_assert_eq(pthread_create(&compaction_tid, NULL, compaction_thread, NULL), 0);
	igt_assert_eq(pthread_create(&frag_tid, NULL, fragmentation_thread, NULL), 0);
	pthread_barrier_wait(&barrier);

	/* Let threads create pressure before rebinding starts */
	sleep(PRESSURE_WARMUP_SECS);

	/*
	 * Rebind buffer[0] in a tight loop.  With userptr dominating RAM,
	 * the kernel is forced to migrate userptr pages during compaction,
	 * which creates a window where the rebind path and the compaction
	 * path compete for the same locks.
	 */
	for (iter = 0; iter < EXEC_ITERATIONS; iter++) {
		uint64_t addr = base_addr;

		sync.flags = DRM_XE_SYNC_FLAG_SIGNAL;
		xe_vm_unbind_async(fd, vm, 0, 0, addr, USERPTR_BUFFER_SIZE, &sync, 1);
		igt_assert(syncobj_wait(fd, &sync.handle, 1, INT64_MAX, 0, NULL));
		syncobj_reset(fd, &sync.handle, 1);

		sync.flags = DRM_XE_SYNC_FLAG_SIGNAL;
		xe_vm_bind_userptr_async(fd, vm, 0,
					 to_user_pointer(userptr_buffers[0]),
					 addr, USERPTR_BUFFER_SIZE, &sync, 1);
		igt_assert(syncobj_wait(fd, &sync.handle, 1, INT64_MAX, 0, NULL));
		syncobj_reset(fd, &sync.handle, 1);

		if (iter % LOG_INTERVAL == 0)
			igt_debug("  Iteration %d/%d\n", iter, EXEC_ITERATIONS);
	}

	stop_threads = true;
	igt_assert_eq(pthread_join(compaction_tid, NULL), 0);
	igt_assert_eq(pthread_join(frag_tid, NULL), 0);
	pthread_barrier_destroy(&barrier);

	/* Unbind and free all buffers */
	for (i = 0; i < num_buffers; i++) {
		uint64_t addr = base_addr + (uint64_t)i * USERPTR_BUFFER_SIZE * 2;

		sync.flags = DRM_XE_SYNC_FLAG_SIGNAL;
		xe_vm_unbind_async(fd, vm, 0, 0, addr, USERPTR_BUFFER_SIZE, &sync, 1);
		igt_assert(syncobj_wait(fd, &sync.handle, 1, INT64_MAX, 0, NULL));
		syncobj_reset(fd, &sync.handle, 1);

		munmap(userptr_buffers[i], USERPTR_BUFFER_SIZE);
	}

	syncobj_destroy(fd, sync.handle);
	xe_vm_destroy(fd, vm);
}

/**
 * SUBTEST: rebind-under-compaction
 * Description: Fills ~70% of available RAM with userptr-backed buffers, then
 *              starts background threads that continuously compact and fragment
 *              physical memory.  While that pressure runs, one buffer is
 *              repeatedly unbound and rebound.  When userptr dominates physical
 *              memory the kernel is forced to migrate userptr pages during
 *              compaction, which creates a window where the userptr rebind path
 *              and the compaction path compete for the same locks.  A correct
 *              kernel completes this within a bounded time; a kernel with a
 *              lock-ordering bug will stall indefinitely.
 *              Written to catch regressions like the one fixed in
 *              "drm/xe/userptr: fix notifier vs folio deadlock".
 *              See: https://gitlab.freedesktop.org/drm/xe/kernel/-/issues/4765
 * Functionality: userptr lock ordering under memory pressure
 * Test category: stress test
 */

int igt_main()
{
	int fd;

	igt_fixture() {
		fd = drm_open_driver(DRIVER_XE);
		xe_device_get(fd);
	}

	igt_subtest_group() {
		igt_fixture() {
			igt_require(getuid() == 0);
			igt_require(igt_get_avail_ram_mb() >= 1024);

			/* Restore THP defrag both in normal teardown and on process exit. */
			if (save_thp_defrag())
				igt_install_exit_handler(thp_defrag_exit_handler);
			write_thp_defrag("always", sizeof("always") - 1);
		}

		igt_subtest("rebind-under-compaction") {
			test_rebind_under_compaction(fd);
		}

		igt_fixture() {
			restore_thp_defrag();
		}
	}

	igt_fixture() {
		xe_device_put(fd);
		drm_close_driver(fd);
	}
}
