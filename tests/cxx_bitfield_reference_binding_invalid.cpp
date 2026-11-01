struct BitfieldReferenceValue {
    int value : 3;
};

int main() {
    BitfieldReferenceValue object{};
    int& reference = object.value;
    const int&& rvalue_reference = object.value;
    return reference + rvalue_reference;
}
