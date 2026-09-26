template<typename T>
concept Accepted = true;

int constrained_add(Accepted auto left, Accepted auto right) {
    return left + right;
}

int constrained_reference(const Accepted auto& value) {
    return value;
}

int main() {
    int value = 7;
    return constrained_add(2, 3) == 5 &&
                   constrained_reference(value) == 7
               ? 0
               : 1;
}
