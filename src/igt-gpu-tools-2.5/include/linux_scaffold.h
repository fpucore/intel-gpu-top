/* SPDX-License-Identifier: MIT */

#ifndef _INTEL_GPU_COMMANDS_SCAFFOLD_H_
#define _INTEL_GPU_COMMANDS_SCAFFOLD_H_

#include <stdint.h>
#include <strings.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

static inline s64 sign_extend64(u64 value, int index)
{
	int shift = 63 - index;
	return (s64)(value << shift) >> shift;
}

/* Make IGT build with Kernels < 4.17 */
#define GENMASK(h, l) \
	((~0UL - (1UL << (l)) + 1) & \
	(~0UL >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l) \
	((~0ULL - (1ULL << (l)) + 1) & \
	(~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#define BITS_PER_BYTE 8
#define BITS_PER_TYPE(t) (sizeof(t) * BITS_PER_BYTE)
#define BITS_PER_LONG BITS_PER_TYPE(long)
#define BITS_PER_LONG_LONG BITS_PER_TYPE(long long)

#define __bf_shf(x) (ffsll(x) - 1)

#define FIELD_GET(mask, reg) \
	(typeof(mask))(((reg) & (mask)) >> __bf_shf(mask))

#define BIT_U8(n) ((u8)(1U << (n)))
#define BIT_U16(n) ((u16)(1U << (n)))
#define BIT_U32(n) ((u32)(1U << (n)))
#define BIT_U64(n) ((u64)(1ULL << (n)))

#endif /* _INTEL_GPU_COMMANDS_SCAFFOLD_H_ */
