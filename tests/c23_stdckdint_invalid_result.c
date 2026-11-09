#include <stdckdint.h>

int main(void)
{
    float invalid_result = 0.0f;
    return ckd_add(&invalid_result, 1, 2);
}
