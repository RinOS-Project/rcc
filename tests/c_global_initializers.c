/* Runtime global initialization is emitted into .init_array. */

int seed(void) {
    return 41;
}

int first_global = seed();
int second_global = first_global + 1;

int main(void) {
    return first_global == 41 && second_global == 42 ? 0 : 1;
}
