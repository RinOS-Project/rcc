struct IntCarrier {
    using value_type = int;
};

struct PrivateCarrier {
private:
    using value_type = int;
};

template<typename T>
struct TemplateCarrier {
    using value_type = T;
};

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
int template_probe() {
    return requires { typename T::value_type; } ? 1 : 0;
}

template<typename T>
int nested_template_probe() {
    return requires { typename T::value_type::nested_type; } ? 1 : 0;
}

template<typename T>
int nested_template_id_probe() {
    return requires {
        typename NestedTypeTemplateOuterCarrier<T>::value_type::nested_type;
    } ? 1 : 0;
}

int main() {
    if (nested_template_probe<NestedTypeOuterCarrier>() != 1) return 2;
    if (nested_template_probe<MissingNestedTypeOuterCarrier>() != 0) return 3;
    if (nested_template_id_probe<NestedTypeCarrier>() != 1) return 4;
    if (nested_template_id_probe<MissingNestedTypeCarrier>() != 0) return 5;
    static_assert(requires { typename IntCarrier::value_type; });
    static_assert(!requires { typename IntCarrier::missing_type; });
    static_assert(!requires { typename PrivateCarrier::value_type; });
    static_assert(requires { typename TemplateCarrier<int>::value_type; });
    return requires { typename IntCarrier::value_type; } &&
                   !requires { typename IntCarrier::missing_type; } &&
                   !requires { typename PrivateCarrier::value_type; } &&
                   requires { typename TemplateCarrier<int>::value_type; } &&
                   template_probe<TemplateCarrier<int> >() == 1
        ? 0 : 1;
}
