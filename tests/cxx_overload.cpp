int choose(int value);
long choose(long value);

int pointer_kind(void* value);
long pointer_kind(const void* value);

int ordered(int first, long second);
long ordered(long first, int second);

int null_pointer(void* value);
long null_pointer(int value);

int default_overload(int value, int extra = 5);
long default_overload(long value);

int call_integer_overload(int value) {
    return choose(value);
}

long call_long_overload(long value) {
    return choose(value);
}

int call_mutable_pointer_overload(void* value) {
    return pointer_kind(value);
}

long call_const_pointer_overload(const void* value) {
    return pointer_kind(value);
}

int call_ordered_int_long(int first, long second) {
    return ordered(first, second);
}

long call_ordered_long_int(long first, int second) {
    return ordered(first, second);
}

int call_null_pointer_overload() {
    return null_pointer(nullptr);
}

int call_null_pointer_variable_overload() {
    auto value = nullptr;
    return null_pointer(value);
}

int call_default_argument_overload() {
    return default_overload(3);
}
