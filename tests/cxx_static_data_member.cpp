class StaticCounter {
public:
    inline static int value = 7;

    static int read() {
        return value;
    }
};

class OutOfLineCounter {
public:
    static int value;

    static int read() {
        return value;
    }
};

int OutOfLineCounter::value = 9;

int main() {
    return StaticCounter::read() == 7 && StaticCounter::value == 7 &&
                   sizeof(StaticCounter) == 1 &&
                   OutOfLineCounter::read() == 9 &&
                   OutOfLineCounter::value == 9 &&
                   sizeof(OutOfLineCounter) == 1 ? 0 : 1;
}
