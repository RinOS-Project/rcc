#include <initializer_list>

int choose_narrowing_initializer_list(std::initializer_list<int>);
int choose_narrowing_initializer_list(double);

int main() {
    return choose_narrowing_initializer_list({1.5});
}
