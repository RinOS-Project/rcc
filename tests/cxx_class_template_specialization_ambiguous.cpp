template<typename T>
class Ambiguous {
public:
    int value;
};

template<typename T>
class Ambiguous<T*> {
public:
    int value;
};

template<typename U>
class Ambiguous<U*> {
public:
    int value;
};

int main() {
    Ambiguous<int*> value{1};
    return value.value;
}
