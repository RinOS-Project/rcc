template <typename Owner>
struct ClassTemplateMemberFunctionSpecialization {
    template <typename T>
    int value(T input);

    template <typename T>
    int value(T* input);
};

template <typename Owner>
struct PartialMemberFunctionTemplateOwner {
    template <typename T>
    int value(T input) {
        return static_cast<int>(input) + 1;
    }
};

template <typename Owner>
struct PartialMemberFunctionTemplateOwner<Owner*> {
    template <typename T>
    int value(T input) {
        return static_cast<int>(input) + 2;
    }
};

template <>
template <>
int ClassTemplateMemberFunctionSpecialization<int>::value<double>(
    double input) {
    return static_cast<int>(input) + 40;
}

template <>
template <>
int ClassTemplateMemberFunctionSpecialization<int>::value<double>(
    double* input) {
    return static_cast<int>(*input) + 40;
}

#if defined(RCC_TEST_MISSING_PARTIAL_OWNER_TEMPLATE_HEAD)
template <>
#elif defined(RCC_TEST_EXTRA_PARTIAL_OWNER_TEMPLATE_HEAD)
template <>
template <>
template <>
#else
template <>
template <>
#endif
int PartialMemberFunctionTemplateOwner<int*>::value<char>(char input) {
    return static_cast<int>(input) + 60;
}

template <typename Owner>
template <typename T>
int ClassTemplateMemberFunctionSpecialization<Owner>::value(T input) {
    return static_cast<int>(input) + 1;
}

template <typename Owner>
template <typename T>
int ClassTemplateMemberFunctionSpecialization<Owner>::value(T* input) {
    return static_cast<int>(*input) + 1;
}

int main() {
    ClassTemplateMemberFunctionSpecialization<int> exact_owner;
    ClassTemplateMemberFunctionSpecialization<double> other_owner;
    PartialMemberFunctionTemplateOwner<int*> exact_partial_owner;
    PartialMemberFunctionTemplateOwner<double*> other_partial_owner;
    PartialMemberFunctionTemplateOwner<int> primary_owner;
    double floating = 2.0;
    int integer = 2;
    return exact_owner.value(2.0) == 42 && exact_owner.value(2) == 3 &&
                   exact_owner.value(&floating) == 42 &&
                   exact_owner.value(&integer) == 3 &&
                   other_owner.value(2.0) == 3 &&
                   other_owner.value(&floating) == 3 &&
                   exact_partial_owner.value('A') == 125 &&
                   exact_partial_owner.value(2) == 4 &&
                   other_partial_owner.value('A') == 67 &&
                   primary_owner.value('A') == 66
               ? 0
               : 1;
}
