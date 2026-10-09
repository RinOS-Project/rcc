template <typename T>
struct DependentDmiParameterBase {};

template <typename T>
struct DependentDmiParameter : DependentDmiParameterBase<T> {
    T seed;
    T adjusted = this->seed + 2;
    T unqualified_adjusted = seed + 3;

    explicit DependentDmiParameter(T initial) : seed(initial) {}
};

int main() {
    DependentDmiParameter<int> narrow(11);
    DependentDmiParameter<long long> wide(13);
    return narrow.adjusted == 13 && narrow.unqualified_adjusted == 14 &&
                   wide.adjusted == 15 && wide.unqualified_adjusted == 16
               ? 0
               : 1;
}
