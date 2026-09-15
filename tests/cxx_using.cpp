namespace math {
int plus_one(int value) {
    return value + 1;
}

int times_two(int value) {
    return value * 2;
}
}

class UsingBase {
public:
    int choose(int value, int extra) {
        return value + extra;
    }
};

class UsingDerived : public UsingBase {
public:
    using UsingBase::choose;

    int choose(int value) {
        return value + 10;
    }
};

using namespace math;
using math::times_two;
using Integer = int;

Integer cxx_using_probe(Integer value) {
    return plus_one(value) + times_two(value);
}

int main() {
    UsingDerived value;
    return cxx_using_probe(20) == 61 && value.choose(4) == 14 &&
                   value.choose(4, 2) == 6 ? 0 : 1;
}
