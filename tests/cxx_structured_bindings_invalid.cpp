struct BindingPair {
    int left;
    int right;
};

int invalid_structured_binding() {
    BindingPair pair{1, 2};
    auto [left, right, extra] = pair;
    return left + right + extra;
}

int invalid_structured_binding_direct_list() {
    BindingPair pair{1, 2};
    auto [left, right]{pair, pair};
    return left + right;
}

int main() {
    return invalid_structured_binding();
}
