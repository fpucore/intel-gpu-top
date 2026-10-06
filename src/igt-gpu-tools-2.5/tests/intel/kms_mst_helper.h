/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2025 Intel Corporation
 */

#ifndef KMS_MST_HELPER_H
#define KMS_MST_HELPER_H

#include "igt.h"

int igt_find_all_mst_output_in_topology(int drm_fd, igt_display_t *display,
					igt_output_t *output,
					igt_output_t *mst_outputs[],
					int *num_mst_outputs);
bool igt_display_has_mst_output(igt_display_t *display);
#endif
