/*
 * rn_assert.c - recoverable assertion bookkeeping (JPL rule 5).
 */
#include "rn/rn_types.h"

#ifdef RN_ASSERT_ABORT
#include <stdio.h>
#include <stdlib.h>
#endif

static uint32_t s_count;
static const char *s_file = "";
static int32_t s_line;

bool rn_assert_fail(const char *file, int32_t line)
{
    s_count++;
    s_file = (file != NULL) ? file : "";
    s_line = line;
#ifdef RN_ASSERT_ABORT
    (void)fprintf(stderr, "assertion failed: %s:%d\n", s_file, (int)line);
    abort();
#endif
    return false;
}

uint32_t rn_assert_count(void)
{
    return s_count;
}

const char *rn_assert_last_file(void)
{
    return s_file;
}

int32_t rn_assert_last_line(void)
{
    return s_line;
}

void rn_assert_reset(void)
{
    s_count = 0u;
    s_file = "";
    s_line = 0;
}
