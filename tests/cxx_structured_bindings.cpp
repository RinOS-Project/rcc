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
    auto&& [forward_left, forward_right] = original;
    forward_left += 4;
    auto&& [temporary_left, temporary_right] = BindingPair{30, 31};
    temporary_right += 2;
    int values[2] = {4, 5};
    auto [first, second] = values;
    first += 1;
    return left == 12 && right == 20 && original.left == 14 &&
           alias_left == 14 && alias_right == 23 && forward_left == 14 &&
           forward_right == 23 && temporary_left == 30 &&
           temporary_right == 33 && first == 5 && second == 5 ? 0 : 1;
}
