int invalid_nonmutable_capture() {
    int value = 1;
    return [value]() {
        value += 1;
        return value;
    }();
}

int main() {
    return invalid_nonmutable_capture();
}
