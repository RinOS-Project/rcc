typedef long long test_v2di __attribute__((__vector_size__(16)));

test_v2di rcc_move_epi64_gate(test_v2di source) {
    return __builtin_ia32_movq128(source);
}
