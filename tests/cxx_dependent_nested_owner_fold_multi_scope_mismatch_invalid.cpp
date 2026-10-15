bool nested_multi_scope_fold_equal(int outer, int middle) {
    return outer == middle;
}

template <int... OuterValues>
struct NestedMultiScopeFoldMismatch {
    template <int... MiddleValues>
    struct Middle {
        static bool values_match() {
            return (nested_multi_scope_fold_equal(OuterValues, MiddleValues) &&
                    ...);
        }
    };
};

int main() {
    using mismatched_middle =
        typename NestedMultiScopeFoldMismatch<1, 2>::template Middle<1>;
    return mismatched_middle::values_match() ? 0 : 1;
}
