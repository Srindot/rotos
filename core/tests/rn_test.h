/*
 * rn_test.h - minimal unit test harness (no framework, no allocation).
 */
#ifndef RN_TEST_H
#define RN_TEST_H

#include <stdio.h>

extern int rn_test_failures;
extern int rn_test_checks;

#define CHECK(cond)                                                              \
    do {                                                                         \
        rn_test_checks++;                                                        \
        if (!(cond)) {                                                           \
            rn_test_failures++;                                                  \
            (void)printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, tol) CHECK(((a) - (b)) <= (tol) && ((b) - (a)) <= (tol))

void test_protocol(void);
void test_geo(void);
void test_route(void);
void test_mappack(void);
void test_mapgen(void);
void test_engine(void);
void test_inflate(void);
void test_delta(void);

#endif
