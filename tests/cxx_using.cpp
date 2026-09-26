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
    using Integer = unsigned short;
    Integer local = (Integer)value;
    {
        using Integer = int;
        Integer shadowed = 5;
        local = (Integer)(local + shadowed);
    }
    return plus_one((int)local) + times_two((int)local);
}

int cxx_using_type_scope_probe(void) {
    return sizeof(Integer) == sizeof(int) ? 0 : 1;
}

int main() {
    UsingDerived value;
    return cxx_using_probe(20) == 76 &&
                   cxx_using_type_scope_probe() == 0 &&
                   value.choose(4) == 14 &&
                   value.choose(4, 2) == 6 ? 0 : 1;
}
