template<typename T, int N>
class Orthogonal {
public:
    int value;
};

template<typename T>
class Orthogonal<T, 4> {
public:
    int value;
};

template<int N>
class Orthogonal<int, N> {
public:
    int value;
};

int main() {
    Orthogonal<int, 4> value{1};
    return value.value;
}
