#include <initializer_list>

int choose_empty_initializer_list(std::initializer_list<int>);
int choose_empty_initializer_list(std::initializer_list<double>);

int main() {
    return choose_empty_initializer_list({});
}
