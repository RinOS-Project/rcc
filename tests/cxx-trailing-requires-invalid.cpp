int rejected_by_trailing_requires(auto value) requires false {
    return value;
}

int main() {
    return rejected_by_trailing_requires(1);
}
