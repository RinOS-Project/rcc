template<class T>
requires requires(T* candidate, __builtin_va_list arguments,
                  __builtin_va_list copy) {
    candidate;
    __builtin_va_arg(arguments, int);
    __builtin_va_end(arguments);
    __builtin_va_copy(copy, arguments);
}
int va_arg_requirement(T value);

class VaArgRequirementHost {
    template<class U>
    requires requires(U* local, __builtin_va_list probe,
                      __builtin_va_list copy) {
        local;
        __builtin_va_arg(probe, int);
        __builtin_va_end(probe);
        __builtin_va_copy(copy, probe);
    }
    friend int va_arg_requirement(U value);
};

template<class V>
requires requires(V* item, __builtin_va_list list, __builtin_va_list copy) {
    item;
    __builtin_va_arg(list, int);
    __builtin_va_end(list);
    __builtin_va_copy(copy, list);
}
int va_arg_requirement(V value) {
    return (int)value;
}

int main() {
    return va_arg_requirement(29) == 29 ? 0 : 1;
}
