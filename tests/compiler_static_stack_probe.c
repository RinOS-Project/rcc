volatile unsigned int compiler_static_stack_probe_sink;

unsigned int compiler_static_stack_probe(void) {
    volatile unsigned char frame[16384];
    frame[0] = 11;
    frame[4095] = 23;
    frame[4096] = 37;
    frame[8192] = 41;
    frame[12288] = 53;
    frame[16383] = 17;
    unsigned int result = (unsigned int)frame[0] + frame[4095] +
        frame[4096] + frame[8192] + frame[12288] + frame[16383];
    compiler_static_stack_probe_sink = result;
    return result;
}

int main(void) {
    return compiler_static_stack_probe() == 182 ? 0 : 1;
}
