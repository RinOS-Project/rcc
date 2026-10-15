template <typename T>
struct NestedFoldTemplateOneByte {
    unsigned char value;
};

template <typename T>
struct NestedFoldTemplateTwoBytes {
    unsigned char value[2];
};

template <template <typename> class... OuterTemplates>
struct NestedFoldTemplateTemplatePackMismatch {
    template <int... InnerValues>
    struct Inner {
        static bool sizes_match() {
            return ((sizeof(OuterTemplates<int>) == InnerValues) && ...);
        }
    };
};

int main() {
    using mismatched_inner = NestedFoldTemplateTemplatePackMismatch<
        NestedFoldTemplateOneByte,
        NestedFoldTemplateTwoBytes>::Inner<1>;
    return mismatched_inner::sizes_match() ? 0 : 1;
}
