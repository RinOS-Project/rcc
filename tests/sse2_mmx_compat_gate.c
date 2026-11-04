typedef long long __m64;
typedef long long __v2di __attribute__((__vector_size__(16)));
typedef double __v2df __attribute__((__vector_size__(16)));
typedef char __v16qi __attribute__((__vector_size__(16)));

__m64 gate_pmuludq(__m64 a, __m64 b) {
    return __builtin_ia32_pmuludq(a, b);
}

__m64 gate_movdq2q(__v2di value) {
    return __builtin_ia32_movdq2q(value);
}

__v2di gate_movq2dq(__m64 value) {
    return __builtin_ia32_movq2dq(value);
}

__m64 gate_cvtpd2pi(__v2df value) {
    return __builtin_ia32_cvtpd2pi(value);
}

__m64 gate_cvttpd2pi(__v2df value) {
    return __builtin_ia32_cvttpd2pi(value);
}

__v2df gate_cvtpi2pd(__m64 value) {
    return __builtin_ia32_cvtpi2pd(value);
}

void gate_maskmove(__v16qi data, __v16qi mask, char* destination) {
    __builtin_ia32_maskmovdqu(data, mask, destination);
}
