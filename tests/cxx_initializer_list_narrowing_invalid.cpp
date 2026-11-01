#include <initializer_list>

struct NarrowingInitializerListPreferred {
    explicit NarrowingInitializerListPreferred(std::initializer_list<int>) {}
    NarrowingInitializerListPreferred(double) {}
};

int main() {
    std::initializer_list<int> values{1.5};
    NarrowingInitializerListPreferred selected{1.5};
    return static_cast<int>(values.size()) + sizeof(selected);
}
