struct Pair {
    int first;
    int second;
};

struct Prefix {
    int prefix;
};

struct Base {
    int base;
};

struct Derived : Prefix, Base {
    int value;
};

Pair global_pair(13, 17);
int global_values[3](2, 3, 5);
Derived global_derived{{19}, {23}, 29};

int main(void) {
    Pair pair(7, 11);
    int values[3](7, 11, 13);
    Derived derived(31, 37, 41);
    return global_pair.first == 13 && global_pair.second == 17 &&
                   global_values[0] == 2 && global_values[1] == 3 &&
                   global_values[2] == 5 && values[0] == 7 &&
                   values[1] == 11 && values[2] == 13 &&
                   pair.first == 7 && pair.second == 11 &&
                   global_derived.prefix == 19 &&
                   global_derived.base == 23 &&
                   global_derived.value == 29 &&
                   derived.prefix == 31 && derived.base == 37 &&
                   derived.value == 41
               ? 0
               : 1;
}
