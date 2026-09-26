class FinalValue final {
public:
    int value;
};

int main() {
    FinalValue value{7};
    return value.value == 7 ? 0 : 1;
}
