typedef unsigned long long u64;
typedef long long s64;

int unsigned_lt(unsigned left, unsigned right) { return left < right; }
int unsigned_gt(unsigned left, unsigned right) { return left > right; }
int unsigned_le(unsigned left, unsigned right) { return left <= right; }
int unsigned_ge(unsigned left, unsigned right) { return left >= right; }

int mixed_int_unsigned_lt(int left, unsigned right) { return left < right; }
int mixed_int_unsigned_gt(int left, unsigned right) { return left > right; }

int signed_wide_unsigned_lt(s64 left, unsigned right) {
    return left < right;
}

int unsigned_wide_signed_lt(u64 left, int right) {
    return left < right;
}

int truth_not(u64 value) { return !value; }
int truth_and(u64 value) { return value && 7; }
int truth_or(u64 value) { return value || 0; }
int truth_conditional(u64 value) { return value ? 13 : 17; }

int truth_if(u64 value) {
    if (value) return 19;
    return 23;
}

int truth_while(u64 value) {
    int count = 0;
    while (value) {
        ++count;
        value = 0;
    }
    return count;
}

int truth_do_while(u64 value) {
    int count = 0;
    do {
        ++count;
        value = 0;
    } while (value);
    return count;
}

int truth_for(u64 value) {
    int count = 0;
    for (; value; value = 0) ++count;
    return count;
}

int main(void)
{
    u64 high_word = 0x100000000ULL;
    if (unsigned_lt(0xffffffffU, 1U) != 0 ||
        unsigned_gt(0xffffffffU, 1U) != 1 ||
        unsigned_le(0xffffffffU, 0xffffffffU) != 1 ||
        unsigned_ge(0xffffffffU, 1U) != 1 ||
        mixed_int_unsigned_lt(-1, 1U) != 0 ||
        mixed_int_unsigned_gt(-1, 1U) != 1 ||
        signed_wide_unsigned_lt(-1, 0xffffffffU) != 1 ||
        unsigned_wide_signed_lt(0, -1) != 1 ||
        truth_not(0) != 1 || truth_not(high_word) != 0 ||
        truth_and(high_word) != 1 || truth_or(high_word) != 1 ||
        truth_conditional(high_word) != 13 || truth_if(high_word) != 19 ||
        truth_while(high_word) != 1 || truth_do_while(high_word) != 1 ||
        truth_for(high_word) != 1) {
        return 1;
    }
    return 0;
}
