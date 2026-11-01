struct BitfieldReferenceValue {
    int value : 3;
};

int select_reference(int&) {
    return 1;
}

int select_reference(const int&) {
    return 2;
}

int main() {
    BitfieldReferenceValue object{};
    return select_reference(object.value);
}
