template<typename T>
int read_rvalue(T&& value) {
    return value;
}

int main() {
    int value = 3;
    return read_rvalue(value);
}
