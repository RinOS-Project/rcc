int choose(int value);
long choose(long value);

int pointer_kind(void* value);
long pointer_kind(const void* value);

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
