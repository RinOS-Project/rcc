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

template <typename Owner>
struct MemberTemplateOperatorOwner {
    template <typename T>
    int operator+(T input) const {
        return static_cast<int>(input) + 2;
    }

    template <typename T>
    int operator+(T* input) const {
        return static_cast<int>(*input) + 3;
    }

    template <typename T>
    int operator[](T input) const {
        return static_cast<int>(input) + 6;
    }

    template <typename T>
    int operator()(T input) const {
        return static_cast<int>(input) + 4;
    }

    template <typename T = char>
    int operator!() const {
        return 9;
    }

    template <typename T = char>
    int operator~() const {
        return 11;
    }
};

struct NonTemplateMemberOperatorOwner {
    template <typename T>
    int operator+(T input) const {
        return static_cast<int>(input) + 5;
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

#if defined(RCC_TEST_MISSING_OPERATOR_TEMPLATE_HEAD)
template <>
#elif defined(RCC_TEST_EXTRA_OPERATOR_TEMPLATE_HEAD)
template <>
template <>
template <>
#else
template <>
template <>
#endif
int MemberTemplateOperatorOwner<int>::operator+<char>(char input) const {
    return static_cast<int>(input) + 50;
}

template <>
template <>
int MemberTemplateOperatorOwner<int>::operator[]<char>(char input) const {
    return static_cast<int>(input) + 80;
}

template <>
template <>
int MemberTemplateOperatorOwner<int>::operator()<char>(char input) const {
    return static_cast<int>(input) + 60;
}

#if defined(RCC_TEST_MISSING_UNARY_OPERATOR_TEMPLATE_HEAD)
template <>
#elif defined(RCC_TEST_EXTRA_UNARY_OPERATOR_TEMPLATE_HEAD)
template <>
template <>
template <>
#else
template <>
template <>
#endif
int MemberTemplateOperatorOwner<int>::operator!<char>() const {
    return 90;
}

template <>
template <>
int MemberTemplateOperatorOwner<int>::operator~<char>() const {
    return 100;
}

template <>
int NonTemplateMemberOperatorOwner::operator+<char>(char input) const {
    return static_cast<int>(input) + 70;
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
    MemberTemplateOperatorOwner<int> operator_owner;
    MemberTemplateOperatorOwner<double> other_operator_owner;
    NonTemplateMemberOperatorOwner non_template_operator_owner;
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
                   primary_owner.value('A') == 66 &&
                   operator_owner + 'A' == 115 && operator_owner + 2 == 4 &&
                   operator_owner + &integer == 5 &&
                   operator_owner['A'] == 145 && operator_owner[2] == 8 &&
                   other_operator_owner + 'A' == 67 &&
                   operator_owner('A') == 125 && operator_owner(2) == 6 &&
                   (!operator_owner) == 90 &&
                   (!other_operator_owner) == 9 &&
                   (~operator_owner) == 100 &&
                   (~other_operator_owner) == 11 &&
                   non_template_operator_owner + 'A' == 135 &&
                   non_template_operator_owner + 2 == 7
               ? 0
               : 1;
}
