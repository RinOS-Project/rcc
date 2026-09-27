int operator""_answer(unsigned long long value) {
    return static_cast<int>(value + 1);
}

int main() {
    return 42_answer == 43 && 42u_answer == 43 ? 0 : 1;
}
