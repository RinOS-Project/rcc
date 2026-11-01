#include <initializer_list>

int choose_initializer_list_rank(std::initializer_list<int>);
int choose_initializer_list_rank(std::initializer_list<double>);

int main() {
    short promoted_integer = 1;
    float promoted_floating = 2.0f;
    return choose_initializer_list_rank(
        {promoted_integer, promoted_floating});
}
