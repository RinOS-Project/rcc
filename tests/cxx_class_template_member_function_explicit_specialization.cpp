template <typename Owner>
struct ClassTemplateMemberFunctionSpecialization {
    template <typename T>
    int value(T input) {
        return static_cast<int>(input) + 1;
    }

    template <typename T>
    int value(T* input) {
        return static_cast<int>(*input) + 1;
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

int main() {
    ClassTemplateMemberFunctionSpecialization<int> exact_owner;
    ClassTemplateMemberFunctionSpecialization<double> other_owner;
    double floating = 2.0;
    int integer = 2;
    return exact_owner.value(2.0) == 42 && exact_owner.value(2) == 3 &&
                   exact_owner.value(&floating) == 42 &&
                   exact_owner.value(&integer) == 3 &&
                   other_owner.value(2.0) == 3 &&
                   other_owner.value(&floating) == 3
               ? 0
               : 1;
}
