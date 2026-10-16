struct ConversionPointerReferenceSource {
    int* value;

    operator int*() const {
        return value;
    }
};

void receive_mutable_pointer_reference(int*& value);

int main() {
    int value = 1;
    ConversionPointerReferenceSource source{&value};
    receive_mutable_pointer_reference(source);
    return 0;
}
