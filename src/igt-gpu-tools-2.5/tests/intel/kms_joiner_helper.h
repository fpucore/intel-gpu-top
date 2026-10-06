/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2025 Intel Corporation
 */

#ifndef KMS_JOINER_HELPER_H
#define KMS_JOINER_HELPER_H

#include "igt_kms.h"
#include "xe/xe_query.h"

void igt_set_all_master_pipes_for_platform(igt_display_t *display,
					   uint32_t *master_pipes);
bool igt_assign_pipes_for_outputs(int drm_fd,
				  igt_output_t **outputs,
				  int num_outputs,
				  int n_pipes,
				  uint32_t *used_pipes_mask,
				  uint32_t master_pipes_mask,
				  uint32_t valid_pipes_mask);
bool intel_max_hdisplay_non_joiner_mode_found(int drm_fd, drmModeConnector *connector,
					      int max_dotclock, drmModeModeInfo *mode);
bool intel_mode_needs_joiner(int drm_fd, drmModeModeInfo *mode);
bool igt_is_joiner_supported_by_source(int drm_fd, enum joined_pipes joiner_type);
const char *igt_get_joined_pipes_name(enum joined_pipes val);

enum force_joiner_mode {
	FORCE_JOINER_ENABLE = 0,
	FORCE_JOINER_DISABLE
};
#endif
