struct Stateful {
    int value;

    Stateful(int initial) {
        this->value = initial;
        this->value += 2;
    }
};

int cxx_constructor_body() {
    Stateful state(40);
    return state.value == 42 ? 0 : 1;
}

int cxx_local_constructor() {
    Stateful state(10);
    return state.value == 12 ? 0 : 1;
}
