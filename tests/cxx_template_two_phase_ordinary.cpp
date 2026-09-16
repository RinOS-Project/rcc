namespace library {
int helper(int value) {
    return value + 10;
}

template<typename T>
int apply(T value) {
    return helper(value);
}

/* This overload is declared after the template definition.  It must not
 * participate in the ordinary unqualified lookup captured at that point. */
int helper(short value) {
    return value + 20;
}
}

int main(void) {
    short value = 3;
    return library::apply(value) == 13 ? 0 : 1;
}
