/**
 * @file test_support.h
 * @brief Shared test infrastructure for the unit test suite
 * 
 * This header provides common macros and counters used across all unit tests,
 * including test execution macros (TEST_START, TEST_PASS, TEST_FAIL) and
 * assertion utilities (ASSERT_EQ, ASSERT_TRUE, ASSERT_FALSE).
 */

#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdio.h>

extern int tests_passed;
extern int tests_failed;

#define TEST_START(name)  printf("  Testing %s... ", name)
#define TEST_PASS()       printf("PASS\n")
#define TEST_FAIL(msg)    do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT_EQ(a, b)   do { if ((a) != (b)) { TEST_FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_TRUE(x)    do { if (!(x)) { TEST_FAIL(#x " is false"); return; } } while(0)
#define ASSERT_FALSE(x)   do { if (x) { TEST_FAIL(#x " is true"); return; } } while(0)

#endif /* TEST_SUPPORT_H */
