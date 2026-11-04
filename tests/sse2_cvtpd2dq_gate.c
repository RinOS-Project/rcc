typedef double test_v2df __attribute__((__vector_size__(16)));
typedef int test_v4si __attribute__((__vector_size__(16)));

test_v4si rcc_cvtpd2dq_gate(test_v2df source) {
    return __builtin_ia32_cvtpd2dq(source);
}
