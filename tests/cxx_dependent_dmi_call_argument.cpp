static int dmi_read_member_argument(int value) {
    return value + 3;
}

template <typename T>
struct DependentDmiCallBase {};

template <typename T>
struct DependentDmiCallArgument : DependentDmiCallBase<T> {
    T member = 12;
    T result = dmi_read_member_argument(this->member);

    DependentDmiCallArgument() {}
};

int main() {
    DependentDmiCallArgument<int> integer;
    DependentDmiCallArgument<long long> wide;
    return integer.result == 15 && wide.result == 15 ? 0 : 1;
}
