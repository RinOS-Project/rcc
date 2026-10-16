struct ScalarConversionReferenceSource {
    operator int() const {
        return 7;
    }
};

void receive_mutable_long_reference(long& value);

int main() {
    ScalarConversionReferenceSource source;
    receive_mutable_long_reference(source);
    return 0;
}
