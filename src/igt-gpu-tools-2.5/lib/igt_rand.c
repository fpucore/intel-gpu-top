#include "igt_rand.h"

/**
 * SECTION:igt_rand
 * @short_description: Random numbers helper library
 * @title: Random
 * @include: igt_rand.h
 */

static uint32_t global = 0x12345678;

uint32_t hars_petruska_f54_1_random_seed(uint32_t new_state)
{
	uint32_t old_state = global;
	global = new_state;
	return old_state;
}

uint32_t hars_petruska_f54_1_random(uint32_t *s)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
	return *s = (*s ^ rol(*s, 5) ^ rol(*s, 24)) + 0x37798849;
#undef rol
}

uint64_t hars_petruska_f54_1_random64(uint32_t *s)
{
	uint32_t l = hars_petruska_f54_1_random(s);
	uint32_t h = hars_petruska_f54_1_random(s);

	return (uint64_t)h << 32 | l;
}

uint32_t hars_petruska_f54_1_random_unsafe(void)
{
	return hars_petruska_f54_1_random(&global);
}

void hars_petruska_f54_1_random_perturb(uint32_t xor)
{
	uint32_t seed = hars_petruska_f54_1_random_seed(0) ^ xor;
	hars_petruska_f54_1_random_seed(seed);
	hars_petruska_f54_1_random_seed(hars_petruska_f54_1_random_unsafe());
}

/* Returns: pseudo-random number in interval [0, ep_ro) */
uint32_t hars_petruska_f54_1_random_unsafe_max(uint32_t ep_ro)
{
	return ((uint64_t)hars_petruska_f54_1_random_unsafe() * ep_ro) >> 32;
}
