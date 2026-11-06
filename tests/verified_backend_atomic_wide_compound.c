#include <stdatomic.h>

int main(void)
{
    _Atomic unsigned long long unsigned_value = 0xffffffffULL;
    _Atomic long long signed_value = 0;
    unsigned int object_evaluations = 0u;
    unsigned int operand_evaluations = 0u;
    unsigned long long unsigned_add;
    unsigned long long unsigned_sub;
    long long signed_add;
    long long signed_sub;

    unsigned_add = (
        *(object_evaluations++, &unsigned_value) +=
        (operand_evaluations++, 1ULL));
    unsigned_sub = (
        *(object_evaluations++, &unsigned_value) -=
        (operand_evaluations++, 1ULL));
    signed_add = signed_value += (operand_evaluations++, -2);
    signed_sub = signed_value -= (operand_evaluations++, -1);

    return unsigned_add == 0x100000000ULL &&
        unsigned_sub == 0xffffffffULL &&
        signed_add == -2 && signed_sub == -1 &&
        object_evaluations == 2u && operand_evaluations == 4u
        ? 0 : 1;
}
