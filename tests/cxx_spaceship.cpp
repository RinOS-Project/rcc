struct Version {
    int value;
};

int operator<=>(Version left, Version right) {
    return left.value - right.value;
}

struct SpaceshipPreferredOverDirectLess {
    int value;
    operator int() const { return value; }
    friend bool operator<(const SpaceshipPreferredOverDirectLess& left,
                          int right) {
        return left.value <= right;
    }
    int operator<=>(const SpaceshipPreferredOverDirectLess& other) const {
        return value - other.value;
    }
};

struct DirectLessWinsRewriteTie {
    int value;
    bool operator<(const DirectLessWinsRewriteTie&) const {
        return false;
    }
    int operator<=>(const DirectLessWinsRewriteTie& other) const {
        return value - other.value;
    }
};

struct ReverseSpaceshipLeft {};
struct ReverseSpaceshipRight {};

int operator<=>(const ReverseSpaceshipRight&,
                const ReverseSpaceshipLeft&) {
    return 1;
}

struct ComparisonResult {
    int value;
    bool operator<(int zero) const {
        return value < zero;
    }
};

struct ClassResultSpaceship {
    int value;
    ComparisonResult operator<=>(const ClassResultSpaceship& other) const {
        ComparisonResult result = {value - other.value};
        return result;
    }
};

int spaceship_rewrite_checks(Version older, Version newer) {
    return (older < newer) && (newer > older) &&
           (older <= newer) && (newer >= older) &&
           (older != newer) && (older == older);
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
    SpaceshipPreferredOverDirectLess equal_left{11};
    SpaceshipPreferredOverDirectLess equal_right{11};
    DirectLessWinsRewriteTie tie_left{1};
    DirectLessWinsRewriteTie tie_right{2};
    ReverseSpaceshipLeft reverse_left;
    ReverseSpaceshipRight reverse_right;
    ClassResultSpaceship class_result_left{2};
    ClassResultSpaceship class_result_right{5};
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
           ((&pointer_value <=> &pointer_value) == 0) &&
           !(equal_left < equal_right) &&
           !(tie_left < tie_right) &&
           (reverse_left < reverse_right) &&
           (reverse_left <= reverse_right) &&
           !(reverse_left >= reverse_right) &&
           (reverse_right > reverse_left) &&
           !(reverse_right <= reverse_left) &&
           (reverse_right >= reverse_left) &&
           (class_result_left < class_result_right) &&
           spaceship_rewrite_checks(older, newer) ? 0 : 1;
}
