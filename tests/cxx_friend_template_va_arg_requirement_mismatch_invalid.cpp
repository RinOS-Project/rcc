template<class T>
requires requires(T* candidate, __builtin_va_list arguments,
                  __builtin_va_list copy) {
    candidate;
    __builtin_va_arg(arguments, int);
    __builtin_va_end(arguments);
    __builtin_va_copy(copy, arguments);
}
int va_arg_requirement_mismatch(T value);

class VaArgRequirementMismatchHost {
    template<class U>
    requires requires(U* local, __builtin_va_list probe,
                      __builtin_va_list copy) {
        local;
        __builtin_va_arg(probe, int);
        __builtin_va_end(probe);
        __builtin_va_copy(copy, probe);
    }
    friend int va_arg_requirement_mismatch(U value);
};

template<class V>
requires requires(V* item, __builtin_va_list list, __builtin_va_list copy) {
    item;
    __builtin_va_arg(list, long);
    __builtin_va_end(list);
    __builtin_va_copy(copy, list);
}
int va_arg_requirement_mismatch(V value) {
    return (int)value;
}

int main() {
    return va_arg_requirement_mismatch(31) == 31 ? 0 : 1;
}
