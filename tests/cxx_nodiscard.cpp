[[nodiscard("must use the result")]] int must_use(int value) {
    return value + 1;
}

class Probe {
public:
    [[nodiscard]] int value() const {
        return 7;
    }
};

int main() {
    Probe probe{};
    must_use(1);
    (void)must_use(2);
    probe.value();
    int used = must_use(3);
    return used == 4 ? 0 : 1;
}
