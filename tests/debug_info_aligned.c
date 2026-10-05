int debug_aligned_frame(void)
{
    _Alignas(16) int value = 7;
    return value;
}
