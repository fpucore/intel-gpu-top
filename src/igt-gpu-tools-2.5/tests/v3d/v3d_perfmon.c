// SPDX-License-Identifier: MIT
/*
 * Copyright © 2022 Igalia S.L.
 * Copyright © 2026 Raspberry Pi Ltd
 */

#include <sys/ioctl.h>

#include "igt.h"
#include "igt_syncobj.h"
#include "igt_v3d.h"
#include "sw_sync.h"

#define V3D_PERFMON_NUM_CL_JOBS 10
#define V3D_PERFMON_NUM_CSD_JOBS 100

IGT_TEST_DESCRIPTION("Tests for the V3D's performance monitors");

int igt_main()
{
	int fd, ver;

	igt_fixture() {
		fd = drm_open_driver(DRIVER_V3D);
		ver = igt_v3d_get_version(fd);
		igt_require(igt_v3d_get_param(fd, DRM_V3D_PARAM_SUPPORTS_PERFMON));
	}

	igt_describe("Make sure a perfmon cannot be created with zero counters.");
	igt_subtest("create-perfmon-0") {
		struct drm_v3d_perfmon_create create = {
			.ncounters = 0,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_CREATE, &create, EINVAL);
	}

	igt_describe("Make sure a perfmon cannot be created with more counters than the maximum allowed.");
	igt_subtest("create-perfmon-exceed") {
		struct drm_v3d_perfmon_create create = {
			.ncounters = DRM_V3D_MAX_PERF_COUNTERS + 1,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_CREATE, &create, EINVAL);
	}

	igt_describe("Make sure a perfmon cannot be created with invalid counters identifiers.");
	igt_subtest("create-perfmon-invalid-counters") {
		struct drm_v3d_perfmon_create create = {
			.ncounters = 1,
		};
		uint32_t total_perfcnt_num;

		total_perfcnt_num = igt_v3d_get_param(fd, DRM_V3D_PARAM_MAX_PERF_COUNTERS);
		igt_assert(total_perfcnt_num || errno != EINVAL);

		/* Fallback if kernel < v6.11 */
		if (!total_perfcnt_num)
			total_perfcnt_num = V3D_PERFCNT_NUM;

		create.counters[0] = total_perfcnt_num;

		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_CREATE, &create, EINVAL);
	}

	igt_describe("Make sure a perfmon with 1 counter can be created.");
	igt_subtest("create-single-perfmon") {
		uint8_t counters[] = { V3D_PERFCNT_FEP_VALID_PRIMTS_NO_PIXELS };
		uint32_t id = igt_v3d_perfmon_create(fd, 1, counters);

		igt_v3d_perfmon_destroy(fd, id);
	}

	igt_describe("Make sure that two perfmons can be created simultaneously.");
	igt_subtest("create-two-perfmon") {
		uint8_t counters_perfmon1[] = { V3D_PERFCNT_AXI_WRITE_STALLS_WATCH_0 };
		uint8_t counters_perfmon2[] = { V3D_PERFCNT_L2T_TMUCFG_READS, V3D_PERFCNT_CORE_MEM_WRITES };

		/* Create two different performance monitors */
		uint32_t id1 = igt_v3d_perfmon_create(fd, 1, counters_perfmon1);
		uint32_t id2 = igt_v3d_perfmon_create(fd, 2, counters_perfmon2);

		/* Make sure that the id's of the performance monitors are different */
		igt_assert_neq(id1, id2);

		igt_v3d_perfmon_destroy(fd, id1);

		/* Make sure that the second perfmon it is still acessible */
		igt_v3d_perfmon_get_values(fd, id2, NULL);

		igt_v3d_perfmon_destroy(fd, id2);
	}

	igt_describe("Make sure that getting the values from perfmon fails for non-zero pad.");
	igt_subtest("get-values-invalid-pad") {
		struct drm_v3d_perfmon_get_values get = {
			.pad = 1,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_GET_VALUES, &get, EINVAL);
	}

	igt_describe("Make sure that getting the values from perfmon fails for invalid identifier.");
	igt_subtest("get-values-invalid-perfmon") {
		struct drm_v3d_perfmon_get_values get = {
			.id = 1,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_GET_VALUES, &get, EINVAL);
	}

	igt_describe("Make sure that getting the values from perfmon fails for invalid memory pointer.");
	igt_subtest("get-values-invalid-pointer") {
		uint8_t counters[] = { V3D_PERFCNT_TLB_QUADS_STENCIL_FAIL,
				       V3D_PERFCNT_PTB_PRIM_VIEWPOINT_DISCARD,
				       V3D_PERFCNT_QPU_UC_HIT };
		uint32_t id = igt_v3d_perfmon_create(fd, 3, counters);

		struct drm_v3d_perfmon_get_values get = {
			.id = id,
			.values_ptr = 0ULL
		};

		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_GET_VALUES, &get, EFAULT);

		igt_v3d_perfmon_destroy(fd, id);
	}

	igt_describe("Sanity check for getting the values from a valid perfmon.");
	igt_subtest("get-values-valid-perfmon") {
		uint8_t counters[] = { V3D_PERFCNT_COMPUTE_ACTIVE,
				       V3D_PERFCNT_PTB_MEM_READS,
				       V3D_PERFCNT_CLE_ACTIVE };
		uint32_t id = igt_v3d_perfmon_create(fd, 3, counters);

		igt_v3d_perfmon_get_values(fd, id, NULL);
		igt_v3d_perfmon_destroy(fd, id);
	}

	igt_describe("Make sure the values returned by the perfmon are increasing monotonically.");
	igt_subtest("get-values-check-monotonically") {
		struct v3d_cl_job *job = igt_v3d_noop_job(fd);
		uint32_t id, out_sync;
		uint64_t *prev, *curr;
		uint8_t counters[4];

		/*
		 * Different V3D versions have different performance counters
		 * IDs for the same counters.
		 */
		if (ver >= 71) {
			counters[0] = 3;  /* CLE-bin-thread-active-cycles */
			counters[1] = 64; /* PTB-memory-writes */
			counters[2] = 66; /* core-memory-reads */
			counters[3] = 71; /* PTB-memory-words-writes */
		} else if (ver >= 42) {
			counters[0] = V3D_PERFCNT_CLE_ACTIVE;
			counters[1] = V3D_PERFCNT_PTB_MEM_WRITES;
			counters[2] = V3D_PERFCNT_CORE_MEM_READS;
			counters[3] = V3D_PERFCNT_PTB_W_MEM_WORDS;
		} else {
			igt_abort_on_f(true, "This V3D version is not supported.");
		}

		id = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);

		prev = calloc(ARRAY_SIZE(counters), sizeof(*prev));
		curr = calloc(ARRAY_SIZE(counters), sizeof(*curr));

		igt_v3d_perfmon_get_values(fd, id, prev);

		for (int i = 0; i < ARRAY_SIZE(counters); i++)
			igt_assert_eq(prev[i], 0);

		out_sync = syncobj_create(fd, 0);
		job->submit->out_sync = out_sync;
		job->submit->perfmon_id = id;

		/* Check if the counters are increasing monotonically. */
		for (int i = 0; i < V3D_PERFMON_NUM_CL_JOBS; i++) {
			do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
			igt_assert(syncobj_wait(fd, &out_sync, 1, INT64_MAX, 0, NULL));

			igt_v3d_perfmon_get_values(fd, id, curr);

			for (int j = 0; j < ARRAY_SIZE(counters); j++)
				igt_assert_lt(prev[j], curr[j]);

			igt_swap(prev, curr);
		}

		syncobj_destroy(fd, out_sync);
		igt_v3d_free_cl_job(fd, job);
		igt_v3d_perfmon_destroy(fd, id);

		free(prev);
		free(curr);
	}

	igt_describe("Make sure that destroying a non-existent perfmon fails.");
	igt_subtest("destroy-invalid-perfmon") {
		struct drm_v3d_perfmon_destroy destroy = {
			.id = 1,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_DESTROY, &destroy, EINVAL);
	}

	igt_describe("Make sure that a perfmon is not accessible after being destroyed.");
	igt_subtest("destroy-valid-perfmon") {
		uint8_t counters[] = { V3D_PERFCNT_AXI_WRITE_STALLS_WATCH_1,
				       V3D_PERFCNT_TMU_CONFIG_ACCESSES,
				       V3D_PERFCNT_TLB_PARTIAL_QUADS,
				       V3D_PERFCNT_L2T_SLC0_READS };
		uint32_t id = igt_v3d_perfmon_create(fd, 4, counters);
		struct drm_v3d_perfmon_get_values get = {
			.id = id,
		};

		igt_v3d_perfmon_get_values(fd, id, NULL);

		igt_v3d_perfmon_destroy(fd, id);

		/* Make sure that the id is no longer allocate */
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_GET_VALUES, &get, EINVAL);
	}

	igt_describe("Make sure that setting a global perfmon fails for an invalid flag.");
	igt_subtest("global-perfmon-invalid-flag") {
		struct drm_v3d_perfmon_set_global set_global = {
			.flags = 0xdeadbeef,
		};

		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global, EINVAL);
	}

	igt_describe("Make sure that setting a global perfmon fails for an invalid identifier.");
	igt_subtest("global-perfmon-invalid-perfmon") {
		struct drm_v3d_perfmon_set_global set_global = {
			.id = 0xdeadbeef,
		};

		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global, EINVAL);
	}

	igt_describe("Make sure that clearing a valid identifier that it's not a global perfmon fails.");
	igt_subtest("global-perfmon-clear-unset-perfmon") {
		uint8_t counters[] = { V3D_PERFCNT_AXI_WRITE_STALLS_WATCH_1,
				       V3D_PERFCNT_TMU_CONFIG_ACCESSES,
				       V3D_PERFCNT_TLB_PARTIAL_QUADS,
				       V3D_PERFCNT_L2T_SLC0_READS };
		uint32_t id = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);
		struct drm_v3d_perfmon_set_global set_global = {
			.id = id,
			.flags = DRM_V3D_PERFMON_CLEAR_GLOBAL,
		};

		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global, EINVAL);

		igt_v3d_perfmon_destroy(fd, id);
	}

	igt_describe("Make sure that the IOCTL fails when one sets a global perfmon, but one is already set.");
	igt_subtest("global-perfmon-set-valid-perfmon-twice") {
		uint8_t counters[] = { V3D_PERFCNT_L2T_HITS,  V3D_PERFCNT_L2T_MISSES };
		uint32_t id1 = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);
		uint32_t id2 = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);
		struct drm_v3d_perfmon_set_global set_global = { .id = id1 };

		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		set_global.id = id2;
		do_ioctl_err(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global, EBUSY);

		igt_v3d_perfmon_destroy(fd, id1);
		igt_v3d_perfmon_destroy(fd, id2);
	}

	igt_describe("Make sure that destroying a global perfmon clears it even without "
		     "DRM_V3D_PERFMON_CLEAR_GLOBAL.");
	igt_subtest("global-perfmon-destroy") {
		uint8_t counters[] = { V3D_PERFCNT_AXI_WRITE_STALLS_WATCH_1,
				       V3D_PERFCNT_L2T_SLC0_READS };
		uint32_t id1 = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);
		uint32_t id2 = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);
		struct drm_v3d_perfmon_set_global set_global = { .id = id1 };

		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		igt_v3d_perfmon_destroy(fd, id1);

		/* Returns success as global perfmon was cleared during destroy. */
		set_global.id = id2;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		igt_v3d_perfmon_destroy(fd, id2);
	}

	igt_describe("Make sure that submitting a per-job perfmon is rejected when a "
		     "global perfmon is active, and that the global perfmon keeps tracking.");
	igt_subtest("global-perfmon-precedes-per-job") {
		struct drm_v3d_perfmon_set_global set_global = { 0 };
		struct v3d_cl_job *job = igt_v3d_noop_job(fd);
		uint32_t id_global, id_job, out_sync;
		uint64_t values;
		uint8_t counter;
		int ret;

		/*
		 * Different V3D versions have different performance counters
		 * IDs for the same counters.
		 */
		if (ver >= 71)
			counter = 0; /* cycle-count */
		else if (ver >= 42)
			counter = V3D_PERFCNT_CYCLE_COUNT;
		else
			igt_abort_on_f(true, "This V3D version is not supported.");

		id_global = igt_v3d_perfmon_create(fd, 1, &counter);
		id_job = igt_v3d_perfmon_create(fd, 1, &counter);

		set_global.id = id_global;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		/* Submitting a per-job perfmon while a global one is active must
		 * be rejected: the global perfmon takes precedence.
		 */
		job->submit->perfmon_id = id_job;

		/* Don't use drmIoctl(), as it restarts the ioctl if errno is EAGAIN. */
		ret = ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
		igt_assert_eq(ret, -1);
		igt_assert_eq(errno, EAGAIN);

		/* Submit a plain job (no per-job perfmon). */
		out_sync = syncobj_create(fd, 0);
		job->submit->perfmon_id = 0;
		job->submit->out_sync = out_sync;
		do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
		igt_assert(syncobj_wait(fd, &out_sync, 1, INT64_MAX, 0, NULL));

		/* The global perfmon kept tracking. */
		igt_v3d_perfmon_get_values(fd, id_global, &values);
		igt_assert_lt(0, values);

		igt_v3d_perfmon_destroy(fd, id_global);
		igt_v3d_perfmon_destroy(fd, id_job);

		syncobj_destroy(fd, out_sync);
		igt_v3d_free_cl_job(fd, job);
	}

	igt_describe("Make sure that the global perfmon is tracking all jobs consistently.");
	igt_subtest("global-perfmon-valid-perfmon") {
		struct drm_v3d_perfmon_set_global set_global = { 0 };
		struct v3d_cl_job *job = igt_v3d_noop_job(fd);
		uint64_t *values1, *values2;
		uint32_t id, out_sync;
		uint8_t counters[2];

		/*
		 * Different V3D versions have different performance counters
		 * IDs for the same counters.
		 */
		if (ver >= 71) {
			counters[0] = 0; /* cycle-count */
			counters[1] = 1; /* core-active */
		} else if (ver >= 42) {
			counters[0] = V3D_PERFCNT_CYCLE_COUNT;
			counters[1] = V3D_PERFCNT_CLE_ACTIVE;
		} else {
			igt_abort_on_f(true, "This V3D version is not supported.");
		}

		id = igt_v3d_perfmon_create(fd, ARRAY_SIZE(counters), counters);

		values1 = calloc(ARRAY_SIZE(counters), sizeof(*values1));
		values2 = calloc(ARRAY_SIZE(counters), sizeof(*values2));

		set_global.id = id;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		igt_v3d_perfmon_get_values(fd, id, values1);

		out_sync = syncobj_create(fd, 0);

		/* Submit an arbitrary number of CL jobs to stress the perfcnts. */
		for (int i = 0; i < V3D_PERFMON_NUM_CL_JOBS; i++) {
			/* Attach the out sync to the last job */
			if (i == V3D_PERFMON_NUM_CL_JOBS - 1)
				job->submit->out_sync = out_sync;

			do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
		}

		/* Wait for the last job to complete so counters are captured. */
		igt_assert(syncobj_wait(fd, &out_sync, 1, INT64_MAX, 0, NULL));
		syncobj_destroy(fd, out_sync);

		igt_v3d_perfmon_get_values(fd, id, values2);

		/* As the global perfmon is active, the cycle-count must have
		 * increased with time.
		 */
		for (int i = 0; i < ARRAY_SIZE(counters); i++)
			igt_assert_lt(values1[i], values2[i]);

		set_global.flags = DRM_V3D_PERFMON_CLEAR_GLOBAL;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		igt_v3d_perfmon_get_values(fd, id, values1);

		out_sync = syncobj_create(fd, 0);

		/* Submit an arbitrary number of CL jobs to stress the perfcnts. */
		for (int i = 0; i < V3D_PERFMON_NUM_CL_JOBS; i++) {
			/* Attach the out sync to the last job */
			if (i == V3D_PERFMON_NUM_CL_JOBS - 1)
				job->submit->out_sync = out_sync;

			do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
		}

		/* Wait for the last job to complete so counters are captured. */
		igt_assert(syncobj_wait(fd, &out_sync, 1, INT64_MAX, 0, NULL));
		igt_v3d_perfmon_get_values(fd, id, values2);

		/* As the global perfmon was cleared, the perfmon is inactive
		 * and it must preserve its values.
		 */
		for (int i = 0; i < ARRAY_SIZE(counters); i++)
			igt_assert_eq(values1[i], values2[i]);

		igt_v3d_perfmon_destroy(fd, id);

		syncobj_destroy(fd, out_sync);
		igt_v3d_free_cl_job(fd, job);

		free(values1);
		free(values2);
	}

	igt_describe("Make sure that a global perfmon tracks jobs from other file descriptors.");
	igt_subtest("global-perfmon-multifd") {
		struct drm_v3d_perfmon_set_global set_global = { 0 },
						  set_global2 = { 0 };
		int fd2 = drm_open_driver_render(DRIVER_V3D);
		struct v3d_cl_job *job = igt_v3d_noop_job(fd2);
		uint32_t id1, id2, out_sync;
		uint64_t values1, values2;
		uint8_t counter;

		/*
		 * Different V3D versions have different performance counters
		 * IDs for the same counters.
		 */
		if (ver >= 71)
			counter = 0; /* cycle-count */
		else if (ver >= 42)
			counter = V3D_PERFCNT_CYCLE_COUNT;
		else
			igt_abort_on_f(true, "This V3D version is not supported.");

		/* Create and set global perfmon from fd. */
		id1 = igt_v3d_perfmon_create(fd, 1, &counter);
		set_global.id = id1;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		igt_v3d_perfmon_get_values(fd, id1, &values1);

		/* Submit jobs from the second fd; the global perfmon should track them. */
		out_sync = syncobj_create(fd2, 0);

		for (int i = 0; i < V3D_PERFMON_NUM_CL_JOBS; i++) {
			/* Attach the out sync to the last job */
			if (i == V3D_PERFMON_NUM_CL_JOBS - 1)
				job->submit->out_sync = out_sync;

			do_ioctl(fd2, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
		}

		igt_assert(syncobj_wait(fd2, &out_sync, 1, INT64_MAX, 0, NULL));

		igt_v3d_perfmon_get_values(fd, id1, &values2);
		igt_assert_lt(values1, values2);

		/* fd2 cannot set its own global perfmon while fd's is active. */
		id2 = igt_v3d_perfmon_create(fd2, 1, &counter);
		set_global2.id = id2;
		do_ioctl_err(fd2, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global2, EBUSY);

		igt_v3d_perfmon_destroy(fd2, id2);
		igt_v3d_perfmon_destroy(fd, id1);

		syncobj_destroy(fd2, out_sync);
		igt_v3d_free_cl_job(fd2, job);

		drm_close_driver(fd2);
	}

	igt_describe("Make sure closing a file descriptor with an active global "
		     "perfmon does not crash the driver.");
	igt_subtest("global-perfmon-destroy-multifd") {
		struct drm_v3d_perfmon_set_global set_global = { 0 };
		struct v3d_cl_job *job = igt_v3d_noop_job(fd);
		int fd2 = drm_open_driver(DRIVER_V3D);
		uint32_t id, new_id, out_sync;
		uint8_t counter = 0;

		/* Create and set global perfmon from fd2. */
		id = igt_v3d_perfmon_create(fd2, 1, &counter);
		set_global.id = id;
		do_ioctl(fd2, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		/* Keep HW busy from fd while fd2 is closed. */
		out_sync = syncobj_create(fd, 0);

		for (int i = 0; i < V3D_PERFMON_NUM_CL_JOBS; i++) {
			/* Attach the out sync to the last job */
			if (i == V3D_PERFMON_NUM_CL_JOBS - 1)
				job->submit->out_sync = out_sync;

			do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job->submit);
		}

		/* Closing fd2 must cleanly stop the active global perfmon and
		 * clear the global perfmon without crashing.
		 */
		drm_close_driver(fd2);

		igt_assert(syncobj_wait(fd, &out_sync, 1, INT64_MAX, 0, NULL));

		/* fd can now set a new global perfmon without EBUSY. */
		new_id = igt_v3d_perfmon_create(fd, 1, &counter);

		set_global.id = new_id;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		set_global.flags = DRM_V3D_PERFMON_CLEAR_GLOBAL;
		do_ioctl(fd, DRM_IOCTL_V3D_PERFMON_SET_GLOBAL, &set_global);

		igt_v3d_perfmon_destroy(fd, new_id);

		syncobj_destroy(fd, out_sync);
		igt_v3d_free_cl_job(fd, job);
	}

	igt_describe("Make sure a perfmon CL job is not contaminated by concurrent CSD "
		     "work on a different queue.");
	igt_subtest("perfmon-counter-isolation") {
		struct v3d_csd_job *csd_job = igt_v3d_empty_shader(fd);
		struct v3d_cl_job *cl_job = igt_v3d_noop_job(fd);
		uint32_t id, blocker, cl_out;
		int timeline, fence_fd;
		uint8_t counter;
		uint64_t values;

		igt_require_sw_sync();

		/*
		 * compute-active-cycles is zero during pure CL work and non-zero
		 * while a CSD job is executing. Submitting CSD jobs alongside a
		 * perfmon CL job lets us distinguish "CSD ran while the perfmon
		 * window was open" (isolation broken) from "CSD finished before
		 * the perfmon window opened" (isolation correct).
		 */
		if (ver >= 71)
			counter = 4; /* compute-active-cycles */
		else if (ver >= 42)
			counter = V3D_PERFCNT_COMPUTE_ACTIVE;
		else
			igt_abort_on_f(true, "This V3D version is not supported.");

		id = igt_v3d_perfmon_create(fd, 1, &counter);

		/*
		 * Create an unsignaled fence via sw_sync. All CSD jobs will
		 * wait on this fence and stay queued until we increment the
		 * sw_sync timeline.
		 */
		timeline = sw_sync_timeline_create();
		fence_fd = sw_sync_timeline_create_fence(timeline, 1);

		blocker = syncobj_create(fd, 0);
		syncobj_import_sync_file(fd, blocker, fence_fd);
		close(fence_fd);

		cl_out = syncobj_create(fd, 0);

		/*
		 * Block all CSD jobs with the unsignaled SW fence so they are
		 * still queued when the CL perfmon job is submitted.
		 */
		csd_job->submit->in_sync = blocker;
		for (int i = 0; i < V3D_PERFMON_NUM_CSD_JOBS; i++)
			do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CSD, csd_job->submit);

		/*
		 * Submit the CL job with the perfmon. v3d perfmon serialization
		 * must add a fence dependency on the CSD done fences so the CL
		 * job is forced to wait for all previous jobs to complete before
		 * its perfmon window opens.
		 */
		cl_job->submit->out_sync = cl_out;
		cl_job->submit->perfmon_id = id;
		do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, cl_job->submit);

		/*
		 * With serialization, the CL job waits for all CSD work to finish
		 * first, so the perfmon sees only pure CL activity. Without it,
		 * the two queues would race and CSD cycles would pollute the counter.
		 */
		sw_sync_timeline_inc(timeline, 1);
		igt_assert(syncobj_wait(fd, &cl_out, 1, INT64_MAX, 0, NULL));

		igt_v3d_perfmon_get_values(fd, id, &values);

		/* All CSD activity finished before the perfmon window opened,
		 * so no compute cycles must have been counted during the CL job.
		 */
		igt_assert_eq(values, 0);

		close(timeline);
		igt_v3d_perfmon_destroy(fd, id);

		syncobj_destroy(fd, blocker);
		syncobj_destroy(fd, cl_out);
		igt_v3d_free_cl_job(fd, cl_job);
		igt_v3d_free_csd_job(fd, csd_job);
	}

	igt_describe("Make sure a non-perfmon CL job submitted after a perfmon CL job "
		     "is serialized behind it and does not contaminate its counter values.");
	igt_subtest("perfmon-counter-isolation-subsequent-cl") {
		struct v3d_cl_job *job1 = igt_v3d_noop_job(fd);
		struct v3d_cl_job *job2 = igt_v3d_noop_job(fd);
		uint64_t id, values, values_after_j2;
		uint32_t blocker, out1, out2;
		int timeline, fence_fd;
		uint8_t counter;

		igt_require_sw_sync();

		/*
		 * Different V3D versions have different performance counters IDs
		 * for the same counters.
		 */
		if (ver >= 71)
			counter = 0; /* cycle-count */
		else if (ver >= 42)
			counter = V3D_PERFCNT_CYCLE_COUNT;
		else
			igt_abort_on_f(true, "This V3D version is not supported.");

		id = igt_v3d_perfmon_create(fd, 1, &counter);

		/*
		 * Create an unsignaled fence via sw_sync. job1 will wait on
		 * this fence and stay blocked until we increment the timeline.
		 */
		timeline = sw_sync_timeline_create();
		fence_fd = sw_sync_timeline_create_fence(timeline, 1);

		blocker = syncobj_create(fd, 0);
		syncobj_import_sync_file(fd, blocker, fence_fd);
		close(fence_fd);

		out1 = syncobj_create(fd, 0);
		out2 = syncobj_create(fd, 0);

		/* job1: BIN job is blocked until we signal the blocker. */
		job1->submit->in_sync_bcl = blocker;
		job1->submit->perfmon_id = id;
		job1->submit->out_sync = out1;
		do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job1->submit);

		/* job2: no perfmon, must wait for job1 due to job serialization. */
		job2->submit->out_sync = out2;
		do_ioctl(fd, DRM_IOCTL_V3D_SUBMIT_CL, job2->submit);

		/* job2 is serialized behind job1, so it cannot have completed yet. */
		igt_assert_eq(syncobj_wait_err(fd, &out2, 1, 0, 0), -ETIME);

		/* While job1 has not started, the perfmon must still read zero. */
		igt_v3d_perfmon_get_values(fd, id, &values);
		igt_assert_eq(values, 0);

		/* Release the blocker. */
		sw_sync_timeline_inc(timeline, 1);

		igt_assert(syncobj_wait(fd, &out1, 1, INT64_MAX, 0, NULL));

		/* job1 must have contributed cycles to the perfmon. */
		igt_v3d_perfmon_get_values(fd, id, &values);
		igt_assert_lt(0, values);

		igt_assert(syncobj_wait(fd, &out2, 1, INT64_MAX, 0, NULL));

		/* job2 doesn't have a perfmon attached, so a second read must
		 * return the same value.
		 */
		igt_v3d_perfmon_get_values(fd, id, &values_after_j2);
		igt_assert_eq(values, values_after_j2);

		close(timeline);
		syncobj_destroy(fd, blocker);
		syncobj_destroy(fd, out1);
		syncobj_destroy(fd, out2);

		igt_v3d_perfmon_destroy(fd, id);

		igt_v3d_free_cl_job(fd, job1);
		igt_v3d_free_cl_job(fd, job2);
	}

	igt_fixture()
		drm_close_driver(fd);
}
