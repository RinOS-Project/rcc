volatile unsigned int compiler_static_stack_probe_sink;

#define FIXED_STACK_PROBE(name, extent) \
    static unsigned int name(void) { \
        volatile unsigned char frame[extent]; \
        unsigned int offset; \
        frame[0] = 11u; \
        for (offset = 4096u; offset < (extent); offset += 4096u) \
            frame[offset - 1u] = (unsigned char)(offset / 4096u + 23u); \
        frame[(extent) - 1u] = 17u; \
        if ((extent) == 1u) return frame[0] == 17u ? 0u : 1u; \
        return frame[0] == 11u && frame[(extent) - 1u] == 17u ? 0u : 1u; \
    }

FIXED_STACK_PROBE(stack_probe_fixed_1, 1u)
FIXED_STACK_PROBE(stack_probe_fixed_4095, 4095u)
FIXED_STACK_PROBE(stack_probe_fixed_4096, 4096u)
FIXED_STACK_PROBE(stack_probe_fixed_4097, 4097u)
FIXED_STACK_PROBE(stack_probe_fixed_8191, 8191u)
FIXED_STACK_PROBE(stack_probe_fixed_8192, 8192u)
FIXED_STACK_PROBE(stack_probe_fixed_8193, 8193u)
FIXED_STACK_PROBE(stack_probe_fixed_16383, 16383u)
FIXED_STACK_PROBE(stack_probe_fixed_16384, 16384u)
FIXED_STACK_PROBE(stack_probe_fixed_16385, 16385u)

static unsigned int stack_probe_fixed_overaligned(void)
{
    _Alignas(8192) volatile unsigned char frame[8193];
    unsigned int offset;
    if ((unsigned long)(void*)frame % 8192ul != 0ul) return 1u;
    for (offset = 4096u; offset < sizeof(frame); offset += 4096u)
        frame[offset - 1u] = (unsigned char)(offset / 4096u + 41u);
    frame[0] = 61u;
    frame[sizeof(frame) - 1u] = 67u;
    return frame[0] == 61u && frame[sizeof(frame) - 1u] == 67u
        ? 0u : 2u;
}

unsigned int compiler_static_stack_probe(void)
{
    unsigned int result =
        stack_probe_fixed_1() |
        stack_probe_fixed_4095() |
        stack_probe_fixed_4096() |
        stack_probe_fixed_4097() |
        stack_probe_fixed_8191() |
        stack_probe_fixed_8192() |
        stack_probe_fixed_8193() |
        stack_probe_fixed_16383() |
        stack_probe_fixed_16384() |
        stack_probe_fixed_16385() |
        stack_probe_fixed_overaligned();
    compiler_static_stack_probe_sink = result;
    return result;
}

unsigned int compiler_static_stack_probe_abi(void);

int main(void) {
    return compiler_static_stack_probe_abi() == 0u ? 0 : 1;
}
