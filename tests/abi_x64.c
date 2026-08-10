typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct Pair64 {
    u64 first;
    u64 second;
} Pair64;

static long global_bias = 7;

long sum7(long a, long b, long c, long d, long e, long f, long g) {
    return a + b + c + d + e + f + g + global_bias;
}

long pair_sum(Pair64 pair, long extra) {
    return (long)pair.first + (long)pair.second + extra;
}

long width_sum(u8 first, u16 second, u32 third) {
    return first + second + third;
}

long call_sum7(void) {
    return sum7(1, 2, 3, 4, 5, 6, 7);
}

long call_pair(void) {
    Pair64 pair;
    pair.first = 11;
    pair.second = 13;
    return pair_sum(pair, 17);
}

