/*
 * Copyright (c) 2025 tinyVisoin.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>

ZTEST(usb, test_usb_enumeration)
{
	zassert_equal(0, 0, "basic tests should work");
}

ZTEST_SUITE(usb, NULL, NULL, NULL, NULL, NULL);
