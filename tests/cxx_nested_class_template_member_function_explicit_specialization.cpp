template <typename Owner>
struct NestedClassTemplateMemberFunctionSpecialization {
    template <typename Inner>
    struct Nested {
        template <typename T>
        int value(T input) {
            return static_cast<int>(input) + 1;
        }

        template <typename Deep>
        struct Deeper {
            template <typename T>
            int nested_value(T input) {
                return static_cast<int>(input) + 2;
            }
        };
    };
};

struct NonTemplateOuterNestedMemberFunctionSpecialization {
    template <typename Inner>
    struct Nested {
        template <typename T>
        int value(T input) {
            return static_cast<int>(input) + 1;
        }

        template <typename Deep>
        struct Deeper {
            template <typename T>
            int nested_value(T input) {
                return static_cast<int>(input) + 2;
            }
        };
    };
};

#if defined(RCC_TEST_MISSING_NESTED_OWNER_TEMPLATE_HEAD)
template <>
template <>
template <>
#elif defined(RCC_TEST_EXTRA_NESTED_OWNER_TEMPLATE_HEAD)
template <>
template <>
template <>
template <>
template <>
#else
template <>
template <>
template <>
template <>
#endif
int NestedClassTemplateMemberFunctionSpecialization<int>::Nested<double>::
    Deeper<long>::nested_value<char>(char input) {
    return static_cast<int>(input) + 80;
}

template <>
template <>
template <>
int NestedClassTemplateMemberFunctionSpecialization<int>::Nested<double>::
    value<char>(char input) {
    return static_cast<int>(input) + 40;
}

#if defined(RCC_TEST_MISSING_NON_TEMPLATE_OWNER_TEMPLATE_HEAD)
template <>
#elif defined(RCC_TEST_EXTRA_NON_TEMPLATE_OWNER_TEMPLATE_HEAD)
template <>
template <>
template <>
#else
template <>
template <>
#endif
int NonTemplateOuterNestedMemberFunctionSpecialization::Nested<double>::
    value<char>(char input) {
    return static_cast<int>(input) + 50;
}

template <>
template <>
template <>
int NonTemplateOuterNestedMemberFunctionSpecialization::Nested<double>::
    Deeper<long>::nested_value<char>(char input) {
    return static_cast<int>(input) + 90;
}

int main() {
    NestedClassTemplateMemberFunctionSpecialization<int>::Nested<double>
        exact_owner;
    NestedClassTemplateMemberFunctionSpecialization<double>::Nested<double>
        other_owner;
    NestedClassTemplateMemberFunctionSpecialization<int>::Nested<double>::
        Deeper<long> exact_deeper_owner;
    NonTemplateOuterNestedMemberFunctionSpecialization::Nested<double>
        non_template_outer_owner;
    NonTemplateOuterNestedMemberFunctionSpecialization::Nested<double>::
        Deeper<long> non_template_outer_deeper_owner;
    return exact_owner.value('A') == 105 && exact_owner.value(2) == 3 &&
                   other_owner.value('A') == 66 &&
                   exact_deeper_owner.nested_value('A') == 145 &&
                   exact_deeper_owner.nested_value(2) == 4 &&
                   non_template_outer_owner.value('A') == 115 &&
                   non_template_outer_owner.value(2) == 3 &&
                   non_template_outer_deeper_owner.nested_value('A') == 155 &&
                   non_template_outer_deeper_owner.nested_value(2) == 4
               ? 0
               : 1;
}
