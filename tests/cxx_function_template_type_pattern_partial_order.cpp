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

template<typename T>
int choose_cv_type_pattern(TypePatternPartialOrderBox<T*>) {
    return 3;
}

template<typename T>
int choose_cv_type_pattern(TypePatternPartialOrderBox<const T*>) {
    return 4;
}

struct MemberPointerOrderOwner {
    int integer;
    double floating;
};

template<typename Owner, typename Member>
int choose_member_pointer_type_pattern(Member Owner::*) {
    return 5;
}

template<typename Owner>
int choose_member_pointer_type_pattern(int Owner::*) {
    return 6;
}

int main() {
    return choose_type_pattern_partial_order(
               TypePatternPartialOrderBox<int*>{}) == 2 &&
           choose_cv_type_pattern(
               TypePatternPartialOrderBox<int*>{}) == 3 &&
           choose_cv_type_pattern(
               TypePatternPartialOrderBox<const int*>{}) == 4 &&
           choose_member_pointer_type_pattern(
               &MemberPointerOrderOwner::integer) == 6 &&
           choose_member_pointer_type_pattern(
               &MemberPointerOrderOwner::floating) == 5
        ? 0 : 1;
}
