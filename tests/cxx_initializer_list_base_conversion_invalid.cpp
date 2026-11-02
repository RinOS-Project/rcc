#include <initializer_list>

struct Base {
    int value;
};

struct PrivateDerived : private Base {};
struct Left : Base {};
struct Right : Base {};
struct AmbiguousDerived : Left, Right {};

int choose(std::initializer_list<int>) {
    return 1;
}

int choose(Base value) {
    return value.value;
}

int main() {
    PrivateDerived private_value{};
    AmbiguousDerived ambiguous_value{};
    return choose({private_value}) + choose({ambiguous_value});
}
