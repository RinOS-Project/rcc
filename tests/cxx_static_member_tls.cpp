class ThreadCounter {
public:
    static thread_local int value;

    static int bump() {
        return ++value;
    }
};

thread_local int ThreadCounter::value = 41;

int main() {
    return ThreadCounter::bump() == 42 && ThreadCounter::value == 42 ? 0 : 1;
}
