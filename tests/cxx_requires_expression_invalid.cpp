int main() {
    return requires(int value) { value + 1; } ? 0 : 1;
}
