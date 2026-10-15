struct NestedCompoundOwner {
    using value_type = int;
    int value;
};

struct NestedCompoundSecondOwner {
    using value_type = long;
    long value;
};

struct NestedCompoundTraits {
    using value_type = unsigned int;
};

struct NestedCompoundDeepOwner {
    using traits_type = NestedCompoundTraits;
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

template <typename Owner>
typename Owner::value_type Owner::* nested_member_pointer_identity(
    typename Owner::value_type Owner::* value) {
    return value;
}

template <typename Owner>
typename Owner::traits_type::value_type* nested_deep_pointer_identity(
    typename Owner::traits_type::value_type* value) {
    return value;
}

int main() {
    NestedCompoundOwner first{1};
    NestedCompoundSecondOwner second{2};
    unsigned int deep_value = 3;
    int* first_pointer =
        nested_pointer_identity<NestedCompoundOwner>(&first.value);
    long* second_pointer =
        nested_pointer_identity<NestedCompoundSecondOwner>(&second.value);
    unsigned int* deep_pointer =
        nested_deep_pointer_identity<NestedCompoundDeepOwner>(&deep_value);
    int& first_reference =
        nested_reference_identity<NestedCompoundOwner>(*first_pointer);
    long& second_reference =
        nested_reference_identity<NestedCompoundSecondOwner>(*second_pointer);
    first.value = nested_value_identity<NestedCompoundOwner>(first.value);
    second.value = nested_value_identity<NestedCompoundSecondOwner>(
        second.value);
    return first_reference == 1 && second_reference == 2 &&
                   *deep_pointer == 3 &&
                   nested_member_pointer<NestedCompoundOwner>() == nullptr &&
                   nested_member_pointer<NestedCompoundSecondOwner>() ==
                       nullptr &&
                   nested_member_pointer_identity<NestedCompoundOwner>(
                       nullptr) == nullptr &&
                   nested_member_pointer_identity<
                       NestedCompoundSecondOwner>(nullptr) == nullptr
        ? 0
        : 1;
}
