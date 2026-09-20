#include "ceres/test.h"

int __t_total = 0;
int __t_failed = 0;

int test_summary(void)
{
    if (__t_failed == 0)
    {
        printf("ALL PASSED %d/%d\n", __t_total, __t_total);
        return 0;
    }
    printf("FAILED %d of %d\n", __t_failed, __t_total);
    return 1;
}
