template <typename T>
struct DependentLookupBaseOne {
    T value;

    int choose(T) const {
        return 1;
    }
};

template <typename T>
struct DependentLookupBaseTwo {
    T value;

    int choose(long) const {
        return 2;
    }
};

template <typename T>
struct DependentLookupAmbiguous : DependentLookupBaseOne<T>,
                                  DependentLookupBaseTwo<T> {};

template <typename T>
struct DependentLookupPrivate : private DependentLookupBaseOne<T> {};

template <typename T>
struct DependentLookupProtected : protected DependentLookupBaseOne<T> {};

int main() {
    DependentLookupAmbiguous<int> ambiguous{};
    DependentLookupPrivate<int> private_object{};
    DependentLookupProtected<int> protected_object{};
    int field = ambiguous.value;
    int method = ambiguous.choose(1);
    int private_field = private_object.value;
    int private_method = private_object.choose(1);
    int protected_field = protected_object.value;
    int protected_method = protected_object.choose(1);
    return field + method + private_field + private_method +
           protected_field + protected_method;
}
