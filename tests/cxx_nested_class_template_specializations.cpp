struct NestedSpecializationOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 1; }
    };
};

template <typename Value>
struct NestedSpecializationOwner::Inner<Value*> {
    int result() const { return 2; }
};

template <>
struct NestedSpecializationOwner::Inner<char> {
    int result() const { return 3; }
};

int main() {
    NestedSpecializationOwner::Inner<long> primary;
    NestedSpecializationOwner::Inner<char*> partial;
    NestedSpecializationOwner::Inner<char> explicit_specialization;
    return primary.result() == 1 && partial.result() == 2 &&
                   explicit_specialization.result() == 3
               ? 0
               : 1;
}
