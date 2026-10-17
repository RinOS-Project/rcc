struct ConversionNestedPointerQualificationSource {
    int* value;

    operator int**() {
        return &value;
    }
};

void consume_nested_pointer_qualification(const int** const& value);
void consume_safe_nested_pointer_qualification(
    const int* const* const& value);

#if defined(TEST_NESTED_POINTER_QUALIFICATION_INVALID)
int main() {
    ConversionNestedPointerQualificationSource source{};
    consume_nested_pointer_qualification(source);
    return 0;
}
#endif

#if defined(TEST_NESTED_POINTER_QUALIFICATION_SAFE)
int main() {
    ConversionNestedPointerQualificationSource source{};
    consume_safe_nested_pointer_qualification(source);
    return 0;
}
#endif
