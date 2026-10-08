struct Pair {
    int first;
    int second;
};

Pair make_pair(int value) {
    return {value, value + 1};
}

int sum_pair(const Pair& value) {
    return value.first + value.second;
}

int read_conditional_pair(bool choose_first) {
    return sum_pair(choose_first ? make_pair(10) : make_pair(20));
}

int main() {
    if (read_conditional_pair(true) != 21) return 1;
    return read_conditional_pair(false) != 41;
}
