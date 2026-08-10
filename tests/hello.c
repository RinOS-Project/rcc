/*
 * RCC Test: Hello World
 */

int main(void) {
    const char* msg = "Hello, RinOS!";
    int x = 42;
    int y = x + 10;

    if (y > 50) {
        return 0;
    }

    for (int i = 0; i < 10; i++) {
        x = x + 1;
    }

    return x;
}
