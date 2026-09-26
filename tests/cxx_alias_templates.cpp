struct AliasHolder {
    int value;
};

template<typename T> using Identity = T;
template<typename T> using Pointer = T*;

template<typename T>
struct Box {
    T value;
};

template<typename T> using BoxAlias = Box<T>;

int alias_identity(Identity<int> value) {
    return value + 2;
}

int alias_pointer(Pointer<int> value) {
    return *value;
}

int alias_class(BoxAlias<int>* value) {
    return value->value;
}

int main() {
    BoxAlias<int> box;
    box.value = 40;
    int value = 5;
    return alias_identity(3) == 5 &&
                   alias_pointer(&value) == 5 &&
                   alias_class(&box) == 40
               ? 0 : 1;
}
