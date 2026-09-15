template<typename T>
class Counter {
public:
    inline static int value = 7;

    static int read() {
        return value;
    }
};

template<int N>
class Offset {
public:
    inline static int value = N;

    static int read() {
        return value;
    }
};

int main() {
    return Counter<int>::read() == 7 &&
                   Counter<long>::read() == 7 &&
                   Counter<int>::value == 7 &&
                   Counter<long>::value == 7 &&
                   Offset<3>::read() == 3 &&
                   Offset<5>::value == 5 ? 0 : 1;
}
