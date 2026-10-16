bool& mutable_bool_reference_sink(bool& value);
void mutable_bool_function();

int main() {
    int mutable_bool_array[1] = { 0 };
    int value = 0;
    int* object_pointer = &value;
    void (*function_pointer)() = mutable_bool_function;
    return (int)mutable_bool_reference_sink(mutable_bool_array) +
           (int)mutable_bool_reference_sink(mutable_bool_function) +
           (int)mutable_bool_reference_sink(object_pointer) +
           (int)mutable_bool_reference_sink(function_pointer);
}
