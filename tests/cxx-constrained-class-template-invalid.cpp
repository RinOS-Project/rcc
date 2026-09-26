template<typename T>
requires false
struct DisabledBox {
    int value;
};

DisabledBox<int> box;

int main() {
    return 0;
}
