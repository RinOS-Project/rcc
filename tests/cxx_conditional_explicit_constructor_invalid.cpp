template <typename T>
struct ConditionalConstructor {
    explicit(sizeof(T) > 4) ConditionalConstructor(int) {}
};

int main() {
    ConditionalConstructor<long long> value = 17;
    (void)value;
    return 0;
}
