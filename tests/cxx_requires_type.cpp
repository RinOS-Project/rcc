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

template<typename T>
int template_probe() {
    return requires { typename T::value_type; } ? 1 : 0;
}

int main() {
    static_assert(requires { typename IntCarrier::value_type; });
    static_assert(!requires { typename IntCarrier::missing_type; });
    static_assert(!requires { typename PrivateCarrier::value_type; });
    return requires { typename IntCarrier::value_type; } &&
                   !requires { typename IntCarrier::missing_type; } &&
                   !requires { typename PrivateCarrier::value_type; } &&
                   template_probe<TemplateCarrier<int> >() == 1
        ? 0 : 1;
}
