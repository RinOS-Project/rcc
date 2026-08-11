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
