int pointer_truth(bool value) {
    return value ? 7 : 3;
}

int main() {
    int value = 0;
    return pointer_truth(&value) == 7 ? 0 : 1;
}
