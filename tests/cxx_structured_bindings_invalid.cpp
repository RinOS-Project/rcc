struct BindingPair {
    int left;
    int right;
};

int invalid_structured_binding() {
    BindingPair pair{1, 2};
    auto [left, right, extra] = pair;
    return left + right + extra;
}

int main() {
    return invalid_structured_binding();
}
