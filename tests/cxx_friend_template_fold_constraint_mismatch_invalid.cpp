template<class... T>
requires (((sizeof(T)) + ...) > 0)
int fold_constraint_mismatch(T... values) {
    return 1;
}

template<class... U>
requires (((sizeof(U)) * ...) > 0)
int fold_constraint_mismatch(U... values) {
    return 2;
}

int main() {
    return fold_constraint_mismatch(2, 5L);
}
