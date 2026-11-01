struct BitfieldReferenceValue {
    unsigned int value : 3;
};

int main() {
    BitfieldReferenceValue object{};
    unsigned int& reference = object.value;
    const unsigned int&& rvalue_reference = object.value;
    unsigned int& conditional_reference =
        true ? object.value : object.value;
    unsigned int& comma_reference = (object.value = 2, object.value);
    return reference + rvalue_reference + conditional_reference +
           comma_reference;
}
