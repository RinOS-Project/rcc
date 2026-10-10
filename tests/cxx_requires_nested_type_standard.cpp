struct NestedTypeCarrier {
    using nested_type = long;
};

struct NestedTypeOuterCarrier {
    using value_type = NestedTypeCarrier;
};

struct MissingNestedTypeCarrier {};

struct MissingNestedTypeOuterCarrier {
    using value_type = MissingNestedTypeCarrier;
};

template<typename T>
struct NestedTypeTemplateOuterCarrier {
    using value_type = T;
};

template<typename T>
constexpr bool has_nested_value_type = requires {
    typename T::value_type::nested_type;
};

template<typename T>
constexpr bool has_template_id_nested_value_type = requires {
    typename NestedTypeTemplateOuterCarrier<T>::value_type::nested_type;
};

static_assert(has_nested_value_type<NestedTypeOuterCarrier>);
static_assert(!has_nested_value_type<MissingNestedTypeOuterCarrier>);
static_assert(has_template_id_nested_value_type<NestedTypeCarrier>);
static_assert(!has_template_id_nested_value_type<MissingNestedTypeCarrier>);

int main() {
    return has_nested_value_type<NestedTypeOuterCarrier> &&
                   !has_nested_value_type<MissingNestedTypeOuterCarrier> &&
                   has_template_id_nested_value_type<NestedTypeCarrier> &&
                   !has_template_id_nested_value_type<MissingNestedTypeCarrier>
               ? 0
               : 1;
}
