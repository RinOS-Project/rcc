template<typename T>
class UnsupportedConstraint {
public:
    int value;
};

int probe();

template<typename T> requires (sizeof(T) > 0 && probe())
class UnsupportedConstraint<T> {
public:
    int value;
};

int main() {
    UnsupportedConstraint<int> value{1};
    return value.value;
}
