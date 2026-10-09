struct ConstMemberFunctionOwner {
    int read() const { return 7; }
};

int main() {
    int (ConstMemberFunctionOwner::*nonconst_method)() =
        &ConstMemberFunctionOwner::read;
    return 0;
}
