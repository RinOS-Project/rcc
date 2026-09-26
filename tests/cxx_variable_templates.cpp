template<int N>
constexpr int doubled = N + N;

template<typename T>
constexpr int type_size = sizeof(T);

template<int N>
int mutable_value = N;

int main(void) {
    if (doubled<21> != 42) return 1;
    if (type_size<int> != 4) return 2;
    if (mutable_value<4> != 4) return 3;
    mutable_value<4> = 8;
    if (mutable_value<4> != 8) return 4;
    return 0;
}
