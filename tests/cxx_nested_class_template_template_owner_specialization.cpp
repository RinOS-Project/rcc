template <typename OwnerValue>
struct NestedTemplateSpecializationOwner {
    template <typename Value>
    struct Inner {
        int result() const { return 10; }
    };
};

template <>
template <>
struct NestedTemplateSpecializationOwner<int>::Inner<char> {
    int result() const { return 20; }
};

int main() {
    NestedTemplateSpecializationOwner<long>::Inner<char> primary;
    NestedTemplateSpecializationOwner<int>::Inner<char> specialized;
    return primary.result() == 10 && specialized.result() == 20 ? 0 : 1;
}
