extern "C" int verified_cpp_debug(int value)
{
    int local = value + 3;
    {
        int nested = local + 1;
        return nested;
    }
}

extern "C" int verified_cpp_debug_for_scope(int limit)
{
    int verified_cpp_outer_value = 0;
    for (int verified_cpp_loop_index = 0;
         verified_cpp_loop_index < limit; ++verified_cpp_loop_index) {
        verified_cpp_outer_value += verified_cpp_loop_index;
    }
    return verified_cpp_outer_value;
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
