#include <initializer_list>

struct ExplicitInitializerListOnly {
    explicit ExplicitInitializerListOnly(std::initializer_list<int>) {}
};

static void accept(ExplicitInitializerListOnly value) {
    (void)value;
}

ExplicitInitializerListOnly direct{1, 2};
ExplicitInitializerListOnly copied = {3, 4};

int main() {
    accept({5, 6});
    return 0;
}
