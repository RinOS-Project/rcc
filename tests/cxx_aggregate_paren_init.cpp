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
Derived global_parenthesized_derived(Prefix{31}, Base{37}, 41);

int main(void) {
    Pair pair(7, 11);
    int values[3](7, 11, 13);
    Derived derived(Prefix{43}, Base{47}, 53);
    return global_pair.first == 13 && global_pair.second == 17 &&
                   global_values[0] == 2 && global_values[1] == 3 &&
                   global_values[2] == 5 && values[0] == 7 &&
                   values[1] == 11 && values[2] == 13 &&
                   pair.first == 7 && pair.second == 11 &&
                   global_derived.prefix == 19 &&
                   global_derived.base == 23 &&
                   global_derived.value == 29 &&
                   global_parenthesized_derived.prefix == 31 &&
                   global_parenthesized_derived.base == 37 &&
                   global_parenthesized_derived.value == 41 &&
                   derived.prefix == 43 && derived.base == 47 &&
                   derived.value == 53
               ? 0
               : 1;
}
