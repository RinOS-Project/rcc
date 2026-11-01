#ifdef RCC_TEST_ARRAY_DECAY_TEMPLATE
template<typename T, int N>
int select_array_decay_reference_template(T (&)[N]) { return 3; }

template<typename T>
int select_array_decay_reference_template(T*) { return 4; }

int invoke_array_decay_reference_template_ambiguity() {
    int values[3] = {};
    return select_array_decay_reference_template(values);
}
#else
int select_array_decay_reference(int (&)[3]) { return 1; }
int select_array_decay_reference(int*) { return 2; }

int invoke_array_decay_reference_ambiguity() {
    int values[3] = {};
    return select_array_decay_reference(values);
}
#endif
