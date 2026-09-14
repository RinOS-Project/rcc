/* File-scope compound literals have static storage duration in C17. */

struct StaticPair {
    int first;
    int second;
};

static struct StaticPair* static_pair = &(struct StaticPair){4, 8};

int main(void) {
    return static_pair->first * 10 + static_pair->second == 48 ? 0 : 1;
}
