void rcc_stream_si64_gate(long long* destination, long long value) {
    __builtin_ia32_movnti64(destination, value);
}
