struct Version {
    int value;
};

int operator<=>(Version left, Version right) {
    return left.value - right.value;
}

int builtin_spaceship(int left, int right) {
    return left <=> right;
}

int builtin_spaceship_wide(long long left, long long right) {
    return left <=> right;
}

int main() {
    Version older{2};
    Version newer{5};
    int forward = older <=> newer;
    int reverse = newer <=> older;
    unsigned int unsigned_low = 0;
    unsigned int unsigned_high = 3;
    int pointer_value = 7;
    return forward == -3 && reverse == 3 &&
           builtin_spaceship(2, 5) == -1 &&
           builtin_spaceship(5, 2) == 1 &&
           builtin_spaceship(5, 5) == 0 &&
           builtin_spaceship(unsigned_low, unsigned_high) == -1 &&
           builtin_spaceship(unsigned_high, unsigned_low) == 1 &&
           builtin_spaceship_wide(-5000000000, -4000000000) == -1 &&
           builtin_spaceship_wide(-4000000000, -5000000000) == 1 &&
           ((&pointer_value <=> &pointer_value) == 0) ? 0 : 1;
}
