class NestedTemplateAccess {
    template <typename T>
    struct Hidden {
        T value;
    };
};

NestedTemplateAccess::Hidden<int> inaccessible_value;
