/* File-scope compound literals have static storage duration in C17. */

struct StaticPair {
    int first;
    int second;
};

static struct StaticPair* static_pair = &(struct StaticPair){4, 8};
static int* static_array_element = &((int[3]){1, 7, 3})[1];
static int* static_pair_second = &((struct StaticPair){5, 9}).second;

int main(void) {
    return static_pair->first * 10 + static_pair->second == 48 &&
                   *static_array_element == 7 && *static_pair_second == 9
               ? 0 : 1;
}
