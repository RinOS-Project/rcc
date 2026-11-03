void take_lvalue(int& value) {
    (void)value;
}

void take_rvalue(int&& value) {
    (void)value;
}

struct RvalueQualifiedReferenceReturn {
    int value;

    int&& get() && {
        return static_cast<int&&>(value);
    }
};

int main() {
    int value = 3;
    int& lvalue_reference = 4;
    int&& rvalue_reference = value;
    take_lvalue(5);
    take_rvalue(value);
    RvalueQualifiedReferenceReturn receiver{6};
    receiver.get();
    return lvalue_reference + rvalue_reference;
}
