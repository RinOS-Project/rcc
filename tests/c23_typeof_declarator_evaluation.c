int main(void) {
    int bound = 3;
    typeof(int[bound++]) first, second;
    if (bound != 4) return 10 + bound;
    if (sizeof(first) != 3u * sizeof(int)) return 20;
    if (sizeof(second) != 3u * sizeof(int)) return 30;
    typeof(int[bound++]) third;
    if (bound != 5) return 40 + bound;
    if (sizeof(third) != 4u * sizeof(int)) return 50;
    return 0;
}
