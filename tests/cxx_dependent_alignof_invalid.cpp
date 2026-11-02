struct IncompleteAlignofType;
constexpr unsigned invalid_direct_function_alignof = alignof(void());

template<typename T>
constexpr unsigned dependent_alignof_invalid_type() {
    return alignof(T);
}

constexpr unsigned invalid_incomplete_alignof =
    dependent_alignof_invalid_type<IncompleteAlignofType>();
constexpr unsigned invalid_function_alignof =
    dependent_alignof_invalid_type<void()>();
