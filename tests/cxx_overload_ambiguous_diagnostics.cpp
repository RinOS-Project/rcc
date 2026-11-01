int f(int) { return 1; }
int f(const int&) { return 2; }

int g(int) { return 3; }
int g(int&) { return 4; }

int h(int) { return 5; }
int h(int&&) { return 6; }

int main() {
    int value = 7;
    return f(1) + g(value) + h(2);
}
