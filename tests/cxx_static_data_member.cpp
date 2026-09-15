class StaticCounter {
public:
    inline static int value = 7;

    static int read() {
        return value;
    }
};

int main() {
    return StaticCounter::read() == 7 && StaticCounter::value == 7 &&
                   sizeof(StaticCounter) == 1 ? 0 : 1;
}
