class CopyableValue {
public:
    int value;

    explicit CopyableValue(int input) : value(input) {}
};

int main() {
    CopyableValue source(17);
    CopyableValue copy = source;
    CopyableValue direct(source);
    CopyableValue* heap = new CopyableValue(source);
    int result = copy.value + direct.value + heap->value;
    delete heap;
    return result == 51 ? 0 : 1;
}
