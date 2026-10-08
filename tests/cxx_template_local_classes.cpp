template <typename T>
T local_class_template_value(T value) {
    struct Local {
        char prefix;
        T item;

        T twice() const {
            return item + item;
        }
    };

    Local local = {0, value};
    return local.twice();
}

template <typename T>
int local_class_template_size(T value) {
    struct Local {
        char prefix;
        T item;
    };
    Local local = {0, value};
    return sizeof(local);
}

template <typename T>
struct LocalClassTemplateBase {
    T value;
};

template <typename T>
int local_class_template_dependent_base_size(T value) {
    struct Local : LocalClassTemplateBase<T> {
        char tail;
    };
    (void)value;
    return sizeof(Local);
}

struct LocalClassTemplatePackBaseA {
    int value;
};

struct LocalClassTemplatePackBaseB {
    long long value;
};

template <typename... Bases>
int local_class_template_base_pack_size() {
    struct Local : Bases... {};
    return sizeof(Local);
}

int local_class_template_base_ctor_observed;

template <typename T>
struct LocalClassTemplateCtorBase {
    T value;

    LocalClassTemplateCtorBase(T input) : value(input) {
        local_class_template_base_ctor_observed = (int)input;
    }
};

template <typename T>
int local_class_template_dependent_base_ctor(T value) {
    struct Local : LocalClassTemplateCtorBase<T> {
        Local(T input) : LocalClassTemplateCtorBase<T>(input) {}
    };
    Local local(value);
    return local_class_template_base_ctor_observed;
}

int local_class_template_base_pack_ctor_observed;

struct LocalClassTemplatePackCtorBaseA {
    LocalClassTemplatePackCtorBaseA() {
        local_class_template_base_pack_ctor_observed =
            local_class_template_base_pack_ctor_observed * 10 + 1;
    }
};

struct LocalClassTemplatePackCtorBaseB {
    LocalClassTemplatePackCtorBaseB() {
        local_class_template_base_pack_ctor_observed =
            local_class_template_base_pack_ctor_observed * 10 + 2;
    }
};

template <typename... Bases>
int local_class_template_base_pack_ctor() {
    struct Local : Bases... {
        Local() : Bases()... {}
    };
    Local local;
    return local_class_template_base_pack_ctor_observed;
}

template <typename T>
T nested_local_class_template_value(T value) {
    struct Outer {
        T item;

        T apply() const {
            struct Inner {
                T item;

                T twice() const {
                    return item + item;
                }
            };
            Inner inner = {item};
            return inner.twice();
        }
    };
    Outer outer = {value};
    return outer.apply();
}

template <typename T>
int discarded_local_class_template_branch(T value) {
    if constexpr (sizeof(T) == 4) {
        struct Discarded {
            T item;

            T should_not_be_lowered() const {
                return item;
            }
        };
        Discarded local = {};
        (void)local;
        return -1;
    } else {
        struct Selected {
            T item;
        };
        Selected local = {value};
        return sizeof(local) == sizeof(T) ? 17 : -2;
    }
}

template <typename T>
const void* local_class_template_typeinfo() {
    struct Local {
        T item;
    };
    return &typeid(Local);
}

int main() {
    if (local_class_template_value(13) != 26) return 1;
    if (local_class_template_value(19LL) != 38) return 2;
    if (local_class_template_size(1) != 8) return 3;
    if (local_class_template_size(1LL) != 16) return 4;
    if (local_class_template_dependent_base_size(1) != 8) return 5;
    if (local_class_template_dependent_base_size(1LL) != 16) return 6;
    if (local_class_template_base_pack_size<>() != 1) return 7;
    if (local_class_template_base_pack_size<LocalClassTemplatePackBaseA>() !=
        4) return 8;
    if (local_class_template_base_pack_size<LocalClassTemplatePackBaseA,
                                             LocalClassTemplatePackBaseB>() !=
        16) return 9;
    if (local_class_template_dependent_base_ctor(31) != 31) return 10;
    if (local_class_template_dependent_base_ctor(37LL) != 37) return 11;
    local_class_template_base_pack_ctor_observed = 0;
    if (local_class_template_base_pack_ctor<>() != 0) return 12;
    local_class_template_base_pack_ctor_observed = 0;
    if (local_class_template_base_pack_ctor<LocalClassTemplatePackCtorBaseA>()
        != 1) return 13;
    local_class_template_base_pack_ctor_observed = 0;
    if (local_class_template_base_pack_ctor<LocalClassTemplatePackCtorBaseA,
                                             LocalClassTemplatePackCtorBaseB>()
        != 12) return 14;
    if (nested_local_class_template_value(11) != 22) return 15;
    if (nested_local_class_template_value(23LL) != 46) return 16;
    if (discarded_local_class_template_branch(23LL) != 17) return 17;
    if (local_class_template_typeinfo<int>() !=
        local_class_template_typeinfo<int>()) return 18;
    if (local_class_template_typeinfo<int>() ==
        local_class_template_typeinfo<long long>()) return 19;
    return 0;
}
