struct Prefix {
    int prefix;
};

struct FirstBase {
    int first;
};

struct SecondBase {
    int second;
};

struct Derived : Prefix, FirstBase, SecondBase {
    int member;
};

Derived global_value{{3}, {5}, {7}, 11};

int main() {
    Derived local_value{{13}, {17}, {19}, 23};
    return global_value.prefix == 3 && global_value.first == 5 &&
                   global_value.second == 7 && global_value.member == 11 &&
                   local_value.prefix == 13 && local_value.first == 17 &&
                   local_value.second == 19 && local_value.member == 23
               ? 0
               : 1;
}
