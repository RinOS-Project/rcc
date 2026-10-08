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
    if (nested_local_class_template_value(11) != 22) return 5;
    if (nested_local_class_template_value(23LL) != 46) return 6;
    if (discarded_local_class_template_branch(23LL) != 17) return 7;
    if (local_class_template_typeinfo<int>() !=
        local_class_template_typeinfo<int>()) return 8;
    if (local_class_template_typeinfo<int>() ==
        local_class_template_typeinfo<long long>()) return 9;
    return 0;
}
