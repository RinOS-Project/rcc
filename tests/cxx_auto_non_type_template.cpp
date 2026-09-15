template<auto N>
int auto_constant() {
    return N;
}

template<auto N = 4>
int auto_default_constant(int value) {
    return value + N;
}

extern "C" int main() {
    return auto_constant<7>() == 7 &&
                   auto_default_constant<>(3) == 7 &&
                   auto_default_constant<-2>(9) == 7
               ? 0
               : 1;
}
