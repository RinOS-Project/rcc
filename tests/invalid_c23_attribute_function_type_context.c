[[reproducible]] int c23_not_a_function;
int c23_not_a_function_pointer [[unsequenced]];

void c23_attribute_recovery(void) {
    [[reproducible]] struct c23_incomplete_tag;
    int c23_after_invalid_attribute(void) [[unsequenced]];
}
