struct BindingPair {
    int left;
    int right;
};

int main() {
    BindingPair original{10, 20};
    auto [left, right] = original;
    left += 2;
    auto& [alias_left, alias_right] = original;
    alias_right += 3;
    int values[2] = {4, 5};
    auto [first, second] = values;
    first += 1;
    return left == 12 && right == 20 && original.left == 10 &&
           alias_left == 10 && alias_right == 23 && first == 5 &&
           second == 5 ? 0 : 1;
}
