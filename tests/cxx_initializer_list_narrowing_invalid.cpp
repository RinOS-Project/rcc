#include <initializer_list>

int main() {
    std::initializer_list<int> values{1.5};
    return static_cast<int>(values.size());
}
