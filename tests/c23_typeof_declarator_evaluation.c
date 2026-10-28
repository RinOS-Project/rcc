static int typeof_vla_typedef_capture(void) {
    int bound = 2;
    typedef typeof(int[bound++]) Captured;
    if (bound != 3) return 60 + bound;

    Captured first, second;
    if (bound != 3) return 70 + bound;
    if (sizeof(first) != 2u * sizeof(int)) return 80;
    if (sizeof(second) != 2u * sizeof(int)) return 90;

    bound = 5;
    Captured third;
    if (sizeof(third) != 2u * sizeof(int)) return 100;
    return 0;
}

int main(void) {
    int bound = 3;
    typeof(int[bound++]) first, second;
    if (bound != 4) return 10 + bound;
    if (sizeof(first) != 3u * sizeof(int)) return 20;
    if (sizeof(second) != 3u * sizeof(int)) return 30;
    typeof(int[bound++]) third;
    if (bound != 5) return 40 + bound;
    if (sizeof(third) != 4u * sizeof(int)) return 50;
    return typeof_vla_typedef_capture();
}
