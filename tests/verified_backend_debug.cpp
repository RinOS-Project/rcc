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

static inline int verified_cpp_debug_inline_leaf(int value)
{
    return value + 5;
}

static inline int verified_cpp_debug_inline_middle(int value)
{
    return verified_cpp_debug_inline_leaf(value) + 7;
}

extern "C" int verified_cpp_debug_inline_host(int value)
{
    return verified_cpp_debug_inline_middle(value) * 2;
}

struct VerifiedCppDebugInlineMember
{
    static int call(int value)
    {
        return value + 11;
    }
};

extern "C" int verified_cpp_debug_inline_member_host(int value)
{
    return VerifiedCppDebugInlineMember::call(value);
}

extern "C" int verified_cpp_debug_const_local(int value)
{
    const int verified_cpp_optimized_constant = 31;
    return value + 1;
}
