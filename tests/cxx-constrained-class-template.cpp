template<typename T>
concept EnabledClass = true;

template<typename T>
requires EnabledClass<T>
struct EnabledBox {
    int value;
};

int main() {
    EnabledBox<int> box{9};
    return box.value == 9 ? 0 : 1;
}
