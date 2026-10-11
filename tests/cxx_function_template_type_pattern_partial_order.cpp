template<typename T>
struct TypePatternPartialOrderBox {};

template<typename T>
int choose_type_pattern_partial_order(TypePatternPartialOrderBox<T>) {
    return 1;
}

template<typename T>
int choose_type_pattern_partial_order(TypePatternPartialOrderBox<T*>) {
    return 2;
}

int main() {
    return choose_type_pattern_partial_order(
               TypePatternPartialOrderBox<int*>{}) == 2
        ? 0 : 1;
}
