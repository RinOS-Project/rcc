extern "C" int verified_cpp_debug(int value)
{
    int local = value + 3;
    {
        int nested = local + 1;
        return nested;
    }
}

struct VerifiedCppDebugAligned32 {
    alignas(32) int value;
};

extern "C" int verified_cpp_debug_overaligned_local()
{
    VerifiedCppDebugAligned32 aligned;
    aligned.value = 41;
    return aligned.value;
}
