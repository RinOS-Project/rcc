int debug_aligned_frame(int selector)
{
#if defined(__i386__)
    _Alignas(8192) int value = 7;
#else
    _Alignas(32) int value = 7;
#endif
    if (selector == 0) return value;
    if (selector < 0) return value + 1;
    return value + 2;
}
