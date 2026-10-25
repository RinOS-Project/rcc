class Number {
public:
    int value;

    int operator+(int rhs) {
        return value + rhs;
    }
};

struct Addressable {
    int value;
};

Addressable operator&(Addressable left, Addressable right);

Addressable* address_of_reference(Addressable& value) {
    return &value;
}

struct ReferenceHolder {
    int& value;

    int* address() const {
        return &value;
    }
};

int main() {
    Number number{5};
    Addressable addressable{11};
    int stored = 17;
    ReferenceHolder holder{stored};
    return number + 7 == 12 &&
                   address_of_reference(addressable) == &addressable &&
                   holder.address() == &stored
        ? 0 : 1;
}
