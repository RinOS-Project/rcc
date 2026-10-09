struct NestedAliasRoot {
    using value_type = int;
};

struct NestedAliasMiddle : NestedAliasRoot {};
struct NestedAliasLeaf : NestedAliasMiddle {};

template <typename T>
struct TemplateNestedAliasRoot {
    using value_type = T;
};

template <typename T>
struct TemplateNestedAliasMiddle : TemplateNestedAliasRoot<T> {};

template <typename T>
struct TemplateNestedAliasLeaf : TemplateNestedAliasMiddle<T> {};

struct SharedNestedAliasRoot {
    using value_type = long;
};

struct SharedNestedAliasLeft : virtual SharedNestedAliasRoot {};
struct SharedNestedAliasRight : virtual SharedNestedAliasRoot {};
struct SharedNestedAliasLeaf : SharedNestedAliasLeft,
                               SharedNestedAliasRight {};

template <typename T>
struct SharedTemplateNestedAliasRoot {
    using value_type = T;
};

template <typename T>
struct SharedTemplateNestedAliasLeft
    : virtual SharedTemplateNestedAliasRoot<T> {};

template <typename T>
struct SharedTemplateNestedAliasRight
    : virtual SharedTemplateNestedAliasRoot<T> {};

template <typename T>
struct SharedTemplateNestedAliasLeaf
    : SharedTemplateNestedAliasLeft<T>, SharedTemplateNestedAliasRight<T> {};

struct ProtectedNestedAliasRoot {
protected:
    using value_type = short;
};

struct ProtectedNestedAliasMiddle : protected ProtectedNestedAliasRoot {};

template <typename T>
struct NestedAliasBox {
    typename T::value_type value;
};

template <typename T>
struct ProtectedNestedAliasReader : T {
    typename T::value_type value;
};

template <typename T>
typename T::value_type function_template_nested_alias() {
    return 17;
}

int main() {
    NestedAliasBox<NestedAliasLeaf> inherited;
    NestedAliasBox<SharedNestedAliasLeaf> shared;
    NestedAliasBox<TemplateNestedAliasLeaf<long long>> templated_inherited;
    NestedAliasBox<SharedTemplateNestedAliasLeaf<char>> templated_shared;
    ProtectedNestedAliasReader<ProtectedNestedAliasMiddle> protected_reader;
    inherited.value = 3;
    shared.value = 5;
    templated_inherited.value = 9;
    templated_shared.value = 'a';
    protected_reader.value = 7;
    return inherited.value == 3 && shared.value == 5 &&
                   templated_inherited.value == 9 &&
                   templated_shared.value == 'a' &&
                   protected_reader.value == 7 &&
                   (64 >> 2) == 16 &&
                   function_template_nested_alias<
                       TemplateNestedAliasLeaf<int>>() == 17
               ? 0
               : 1;
}
