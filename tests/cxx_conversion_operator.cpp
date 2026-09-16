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

long take_long(long value) {
    return value;
}

int choose_conversion(int value) {
    return value + 100;
}

long choose_conversion(long value) {
    return value + 200;
}

int return_integer() {
    Meter meter{9};
    return meter;
}

long return_long() {
    Meter meter{11};
    return meter;
}

int main() {
    Meter meter{7};
    Flag flag{1};
    int direct = meter;
    int argument = take_integer(meter);
    long widened = take_long(meter);

    if (!flag) return 1;
    if (direct != 7 || argument != 7 || widened != 7 ||
        return_integer() != 9 || return_long() != 11 ||
        choose_conversion(meter) != 107) return 1;
    return 0;
}
