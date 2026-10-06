// SPDX-License-Identifier: MIT
/*
 * Copyright © 2026 Google
 *
 * Authors:
 *   Louis Chauvet <louis.chauvet@bootlin.com> (assisted by MistralAI)
 */

#include <stdint.h>
#include <stdlib.h>

#include "drmtest.h"
#include "igt_core.h"
#include "igt_kms.h"

/**
 * TEST: get_array_diff
 * Category: Core
 * Description: Test the get_array_diff() function
 *
 * SUBTEST: get_array_diff-empty-both
 * Description: Test with both arrays empty
 *
 * SUBTEST: get_array_diff-empty-a
 * Description: Test with array_a empty
 *
 * SUBTEST: get_array_diff-empty-b
 * Description: Test with array_b empty
 *
 * SUBTEST: get_array_diff-no-diff
 * Description: Test with identical arrays (no difference)
 *
 * SUBTEST: get_array_diff-full-diff
 * Description: Test with completely different arrays
 *
 * SUBTEST: get_array_diff-partial-diff
 * Description: Test with partial overlap between arrays
 *
 * SUBTEST: get_array_diff-null-diff
 * Description: Test with diff parameter set to NULL (count only)
 */

static void test_get_array_diff_empty_both(void)
{
	uint32_t *diff = NULL;
	int diff_len;

	diff_len = get_array_diff(NULL, 0, NULL, 0, &diff);
	igt_assert_eq(diff_len, 0);
	free(diff);
}

static void test_get_array_diff_empty_a(void)
{
	uint32_t array_b[] = {1, 2, 3};
	uint32_t *diff = NULL;
	int diff_len;

	diff_len = get_array_diff(NULL, 0, array_b, ARRAY_SIZE(array_b), &diff);
	igt_assert_eq(diff_len, 0);
	free(diff);
}

static void test_get_array_diff_empty_b(void)
{
	uint32_t array_a[] = {1, 2, 3};
	uint32_t *diff = NULL;
	int diff_len;

	diff_len = get_array_diff(array_a, ARRAY_SIZE(array_a), NULL, 0, &diff);
	igt_assert_eq(diff_len, 3);
	igt_assert(diff);
	igt_assert_eq(diff[0], 1);
	igt_assert_eq(diff[1], 2);
	igt_assert_eq(diff[2], 3);
	free(diff);
}

static void test_get_array_diff_no_diff(void)
{
	uint32_t array_a[] = {1, 2, 3};
	uint32_t array_b[] = {1, 2, 3};
	uint32_t *diff = NULL;
	int diff_len;

	diff_len = get_array_diff(array_a, ARRAY_SIZE(array_a), array_b, ARRAY_SIZE(array_b),
				  &diff);
	igt_assert_eq(diff_len, 0);
	free(diff);
}

static void test_get_array_diff_full_diff(void)
{
	uint32_t array_a[] = {1, 2, 3};
	uint32_t array_b[] = {4, 5, 6};
	uint32_t *diff = NULL;
	int diff_len;

	diff_len = get_array_diff(array_a, ARRAY_SIZE(array_a), array_b, ARRAY_SIZE(array_b),
				  &diff);
	igt_assert_eq(diff_len, 3);
	igt_assert(diff);
	igt_assert_eq(diff[0], 1);
	igt_assert_eq(diff[1], 2);
	igt_assert_eq(diff[2], 3);
	free(diff);
}

static void test_get_array_diff_partial_diff(void)
{
	uint32_t array_a[] = {1, 2, 3, 4, 5};
	uint32_t array_b[] = {2, 3, 6};
	uint32_t *diff = NULL;
	int diff_len;

	diff_len = get_array_diff(array_a, ARRAY_SIZE(array_a), array_b, ARRAY_SIZE(array_b),
				  &diff);
	igt_assert_eq(diff_len, 3);
	igt_assert(diff);
	igt_assert_eq(diff[0], 1);
	igt_assert_eq(diff[1], 4);
	igt_assert_eq(diff[2], 5);
	free(diff);
}

static void test_get_array_diff_null_diff(void)
{
	uint32_t array_a[] = {1, 2, 3};
	uint32_t array_b[] = {2, 3};
	int diff_len;

	diff_len = get_array_diff(array_a, ARRAY_SIZE(array_a), array_b, ARRAY_SIZE(array_b), NULL);
	igt_assert_eq(diff_len, 1);
}

IGT_TEST_DESCRIPTION("Test get_array_diff() function");
int igt_main()
{
	igt_subtest("get_array_diff-empty-both")
		test_get_array_diff_empty_both();

	igt_subtest("get_array_diff-empty-a")
		test_get_array_diff_empty_a();

	igt_subtest("get_array_diff-empty-b")
		test_get_array_diff_empty_b();

	igt_subtest("get_array_diff-no-diff")
		test_get_array_diff_no_diff();

	igt_subtest("get_array_diff-full-diff")
		test_get_array_diff_full_diff();

	igt_subtest("get_array_diff-partial-diff")
		test_get_array_diff_partial_diff();

	igt_subtest("get_array_diff-null-diff")
		test_get_array_diff_null_diff();
}
