class GeneralConstructor {
public:
    int value;

    GeneralConstructor(int input) : value(input) {
        int delta = input + 1;
        if (delta > 0) {
            value += delta;
        } else {
            value = 0;
        }
    }
};

int main() {
    GeneralConstructor object(3);
    return object.value == 7 ? 0 : 1;
}
