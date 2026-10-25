template <typename Owner>
struct ClassTemplateMemberFunctionSpecialization {
    template <typename T>
    int value(T input) {
        return static_cast<int>(input) + 1;
    }
};

template <>
template <>
int ClassTemplateMemberFunctionSpecialization<int>::value<double>(
    double input) {
    return static_cast<int>(input) + 40;
}

int main() {
    ClassTemplateMemberFunctionSpecialization<int> exact_owner;
    ClassTemplateMemberFunctionSpecialization<double> other_owner;
    return exact_owner.value(2.0) == 42 && exact_owner.value(2) == 3 &&
                   other_owner.value(2.0) == 3
               ? 0
               : 1;
}
