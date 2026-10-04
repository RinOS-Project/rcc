template<typename T>
int select_template(T first, int second) {
    return (int)first + (int)second;
}

template<typename T>
long select_template(int first, T second) {
    return (long)first + (long)second + 100;
}

int main() {
    return (int)select_template((short)1, (short)2);
}
