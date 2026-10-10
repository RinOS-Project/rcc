struct LeftBase {};
struct RightBase {};
struct Derived : LeftBase, RightBase {};

int select_unrelated_pointer_bases(LeftBase*) {
    return 1;
}

int select_unrelated_pointer_bases(RightBase*) {
    return 2;
}

int main() {
    Derived value;
    return select_unrelated_pointer_bases(&value);
}
