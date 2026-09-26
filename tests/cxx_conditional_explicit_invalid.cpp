int runtime_flag();

struct Invalid {
    explicit(runtime_flag()) Invalid(int value) : value(value) {}
    int value;
};

int main() {
    return 0;
}
