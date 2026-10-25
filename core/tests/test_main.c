/*
 * test_main.c - runs every navigation unit test.
 */
#include "rn_test.h"

#include "rn/rn_types.h"

int rn_test_failures;
int rn_test_checks;

int main(void)
{
    test_protocol();
    test_geo();
    test_route();
    test_mappack();
    test_mapgen();
    test_engine();
    test_inflate();
    test_delta();
    (void)printf("%d checks, %d failures, %u assertion failures recorded\n",
                 rn_test_checks, rn_test_failures, (unsigned)rn_assert_count());
    return rn_test_failures == 0 ? 0 : 1;
}
