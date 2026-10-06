/* SPDX-License-Identifier: MIT */
/*
 * Copyright 2026 Advanced Micro Devices, Inc.
 */

#ifndef AMD_VPE_H
#define AMD_VPE_H

#include <amdgpu.h>

/* color space conversion commands
 * source:      1024(pitch 1024) x 256 RGBA
 * destination: 1024(pitch 1024) x 256 ARGB
 */

#define SRC_PLANE_PATTERN	0x12345678
#define DST_PLANE_PATTERN	0xff123456

#define VPEC_CSC_CMD_LENGTH	12
#define VPEP_CSC_CMD_LENGTH	154
#define VPEC_CSC_CMD_SIZE	(VPEC_CSC_CMD_LENGTH * sizeof(uint32_t))
#define VPEP_CSC_CMD_SIZE	(VPEP_CSC_CMD_LENGTH * sizeof(uint32_t))

/* horizontal mirror commands
 * source:      1024(pitch 1024) x 256 RGBA
 * destination: 1024(pitch 1024) x 256 RGBA
 */

#define LEFT_PLANE_PATTERN	0xff123456
#define RIGHT_PLANE_PATTERN	0xff654321

#define VPEC_HMIRROR_CMD_LENGTH	38
#define VPEP_HMIRROR_CMD_LENGTH	1661
#define VPEC_HMIRROR_CMD_SIZE	(VPEC_HMIRROR_CMD_LENGTH * sizeof(uint32_t))
#define VPEP_HMIRROR_CMD_SIZE	(VPEP_HMIRROR_CMD_LENGTH * sizeof(uint32_t))

uint32_t *vpe_get_vpec_csc_cmd(void);
uint32_t *vpe_get_vpep_csc_cmd(void);
uint32_t *vpe_get_vpec_hmirror_cmd(void);
uint32_t *vpe_get_vpep_hmirror_cmd(void);

#endif /* AMD_VPE_H */
