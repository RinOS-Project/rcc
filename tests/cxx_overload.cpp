int choose(int value);
long choose(long value);

int pointer_kind(void* value);
long pointer_kind(const void* value);

int ordered(int first, long second);
long ordered(long first, int second);

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
