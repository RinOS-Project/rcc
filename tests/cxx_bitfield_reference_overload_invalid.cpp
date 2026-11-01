struct BitfieldReferenceValue {
    unsigned int value : 3;
};

int select_reference(unsigned int&) {
    return 1;
}

int select_reference(const unsigned int&) {
    return 2;
}

int main() {
    BitfieldReferenceValue object{};
    return select_reference(object.value);
}
