inline int cxx_inline_value = 7;
inline const int cxx_inline_constant = 5;

inline int cxx_inline_increment(int value) {
    return value + 1;
}

int main() {
    return cxx_inline_value + cxx_inline_constant == 12 &&
                   cxx_inline_increment(cxx_inline_value) == 8
               ? 0
               : 1;
}
