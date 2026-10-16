bool& mutable_bool_reference_sink(bool& value);
int mutable_bool_array[1];
void mutable_bool_function();

int main() {
    return (int)mutable_bool_reference_sink(mutable_bool_array) +
           (int)mutable_bool_reference_sink(mutable_bool_function);
}
