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
    if (local_class_template_typeinfo<int>() !=
        local_class_template_typeinfo<int>()) return 5;
    if (local_class_template_typeinfo<int>() ==
        local_class_template_typeinfo<long long>()) return 6;
    return 0;
}
