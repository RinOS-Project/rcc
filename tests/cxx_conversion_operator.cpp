class Meter {
public:
    int value;

    operator int() {
        return value;
    }
};

class Flag {
public:
    int value;

    explicit operator bool() {
        return value != 0;
    }
};

int take_integer(int value) {
    return value;
}

int return_integer() {
    Meter meter{9};
    return meter;
}

int main() {
    Meter meter{7};
    Flag flag{1};
    int direct = meter;
    int argument = take_integer(meter);

    if (!flag) return 1;
    if (direct != 7 || argument != 7 || return_integer() != 9) return 1;
    return 0;
}
