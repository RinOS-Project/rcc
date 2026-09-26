struct BindingPair {
    int left;
    int right;
};

int invalid_structured_binding() {
    BindingPair pair{1, 2};
    auto&& [left, right] = pair;
    return left + right;
}

int main() {
    return invalid_structured_binding();
}
