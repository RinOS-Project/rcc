struct Counter {
    static int add(int first, int second) {
        return first + second;
    }

    static int add_five(int value) {
        return add(value, 5);
    }
};

namespace api {
struct Counter {
    static int triple(int value) {
        return value * 3;
    }
};
}

int cxx_static_member_probe() {
    Counter value;
    return Counter::add(19, 23) + value.add_five(18) - api::Counter::triple(14);
}

int main() {
    return cxx_static_member_probe() == 23 ? 0 : 1;
}
