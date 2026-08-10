/* Simple test for RCC without inline asm */

int global_var = 42;
int array[10];

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

void test_loop(void) {
    for (int i = 0; i < 10; i++) {
        array[i] = i * 2;
    }
}

int main(void) {
    int x = add(3, 4);
    int y = multiply(x, 2);
    int z = factorial(5);
    test_loop();
    return x + y + z + global_var;
}
