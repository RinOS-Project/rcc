template <typename... OuterTypes>
struct NestedOwnerFoldPackMismatch {
    template <int... InnerValues>
    struct Inner {
        static bool widths_match() {
            return ((sizeof(OuterTypes) == InnerValues) && ...);
        }
    };
};

int main() {
    using mismatched_inner = typename NestedOwnerFoldPackMismatch<
        short, char>::template Inner<2>;
    mismatched_inner value;
    return value.widths_match() ? 0 : 1;
}
