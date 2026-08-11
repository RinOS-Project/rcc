typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

u64 abi_wide_global = (u64)0x1234567889abcdef;

u64 abi_wide_literal(void) {
    return (u64)0x1234567889abcdef;
}

u64 abi_wide_identity(u64 value) {
    return value;
}

u64 abi_wide_add(u64 left, u64 right) {
    return left + right;
}

u64 abi_wide_sub(u64 left, u64 right) {
    return left - right;
}

u64 abi_wide_bits(u64 left, u64 right) {
    return (left & right) | (left ^ right);
}

i64 abi_wide_negate(i64 value) {
    return -value;
}

u64 abi_wide_local(u64 value) {
    u64 local = value;
    return local;
}

u64 abi_wide_assign(u64 value) {
    u64 local = 0;
    local = value;
    return local;
}

u64 abi_wide_assign_narrow(int value) {
    u64 local = 0;
    local = value;
    return local;
}

u64 abi_wide_add_narrow(u64 left, int right) {
    return left + right;
}

int abi_wide_equal(u64 left, u64 right) { return left == right; }
int abi_wide_less_unsigned(u64 left, u64 right) { return left < right; }
int abi_wide_greater_signed(i64 left, i64 right) { return left > right; }
int abi_wide_less_equal_signed(i64 left, i64 right) { return left <= right; }

u64 abi_wide_shift_left(u64 value, int count) { return value << count; }
u64 abi_wide_shift_right(u64 value, int count) { return value >> count; }
i64 abi_wide_shift_right_signed(i64 value, int count) {
    return value >> count;
}

u64 abi_wide_multiply(u64 left, u64 right) { return left * right; }
i64 abi_wide_multiply_signed(i64 left, i64 right) { return left * right; }
u64 abi_wide_divide(u64 left, u64 right) { return left / right; }
u64 abi_wide_modulo(u64 left, u64 right) { return left % right; }
i64 abi_wide_divide_signed(i64 left, i64 right) { return left / right; }
i64 abi_wide_modulo_signed(i64 left, i64 right) { return left % right; }

u64 abi_wide_call(void) {
    return abi_wide_add((u64)0x00000001ffffffff, (u64)2);
}

u64 abi_wide_call_promoted(void) {
    return abi_wide_identity(7);
}

u64 abi_wide_widen_unsigned(u32 value) {
    return value;
}

i64 abi_wide_widen_signed(int value) {
    return value;
}

u64 abi_wide_global_load(void) {
    return abi_wide_global;
}

u64 abi_wide_global_store(u64 value) {
    abi_wide_global = value;
    return abi_wide_global;
}

u64 abi_atomic_u64_load(volatile u64* value) {
    return __atomic_load_n(value, 2);
}

void abi_atomic_u64_store(volatile u64* value, u64 desired) {
    __atomic_store_n(value, desired, 3);
}

u64 abi_atomic_u64_exchange(volatile u64* value, u64 desired) {
    return __atomic_exchange_n(value, desired, 4);
}

int abi_atomic_u64_compare(volatile u64* value, u64* expected,
                           u64 desired) {
    return __atomic_compare_exchange_n(value, expected, desired, 0, 4, 2);
}
