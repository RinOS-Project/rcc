int debug_aligned_frame(void)
{
#if defined(__i386__)
    _Alignas(8192) int value = 7;
#else
    _Alignas(16) int value = 7;
#endif
    return value;
}
