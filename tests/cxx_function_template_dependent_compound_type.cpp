struct NestedCompoundOwner {
    using value_type = int;
    int value;
};

struct NestedCompoundSecondOwner {
    using value_type = long;
    long value;
};

template <typename Owner>
typename Owner::value_type* nested_pointer_identity(
    typename Owner::value_type* value) {
    return value;
}

template <typename Owner>
typename Owner::value_type& nested_reference_identity(
    typename Owner::value_type& value) {
    return value;
}

template <typename Owner>
typename Owner::value_type nested_value_identity(
    typename Owner::value_type value) {
    return value;
}

template <typename Owner>
typename Owner::value_type Owner::* nested_member_pointer() {
    return nullptr;
}

int main() {
    NestedCompoundOwner first{1};
    NestedCompoundSecondOwner second{2};
    int* first_pointer =
        nested_pointer_identity<NestedCompoundOwner>(&first.value);
    long* second_pointer =
        nested_pointer_identity<NestedCompoundSecondOwner>(&second.value);
    int& first_reference =
        nested_reference_identity<NestedCompoundOwner>(*first_pointer);
    long& second_reference =
        nested_reference_identity<NestedCompoundSecondOwner>(*second_pointer);
    first.value = nested_value_identity<NestedCompoundOwner>(first.value);
    second.value = nested_value_identity<NestedCompoundSecondOwner>(
        second.value);
    return first_reference == 1 && second_reference == 2 &&
                   nested_member_pointer<NestedCompoundOwner>() == nullptr &&
                   nested_member_pointer<NestedCompoundSecondOwner>() ==
                       nullptr
        ? 0
        : 1;
}
