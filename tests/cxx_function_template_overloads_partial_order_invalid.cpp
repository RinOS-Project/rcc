template<typename T>
int select_template(T first, int second) {
    return (int)first + (int)second;
}

template<typename T>
long select_template(int first, T second) {
    return (long)first + (long)second + 100;
}

template<typename T>
int select_incomparable_reference_template(T marker, const int& value) {
    return (int)marker + value;
}

template<typename T>
int select_incomparable_reference_template(T marker, volatile int& value) {
    return (int)marker + value + 100;
}

int main() {
    int value = 3;
    return (int)select_template((short)1, (short)2) +
           select_incomparable_reference_template(1, value);
}
