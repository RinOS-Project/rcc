namespace math {
int plus_one(int value) {
    return value + 1;
}

int times_two(int value) {
    return value * 2;
}
}

using namespace math;
using math::times_two;
using Integer = int;

Integer cxx_using_probe(Integer value) {
    return plus_one(value) + times_two(value);
}

int main() {
    return cxx_using_probe(20) == 61 ? 0 : 1;
}
