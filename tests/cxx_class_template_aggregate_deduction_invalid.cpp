template<typename T>
struct AggregateOnly {
    T value;
};

int main() {
    AggregateOnly value(7);
    return value.value;
}
