struct TransitivePadding {
    int padding;
};

template <typename T>
struct TransitiveRoot {
    T value;

    int select(T amount) const {
        return value + amount;
    }

    int select(double amount) const {
        return value + amount + 1000;
    }
};

template <typename T>
struct TransitiveMiddle : TransitivePadding, TransitiveRoot<T> {};

template <typename T>
struct TransitiveLeaf : TransitivePadding, TransitiveMiddle<T> {
    T read() const {
        return this->value;
    }

    int choose(T amount) const {
        return this->select(amount);
    }
};

template <typename T>
struct TransitiveVirtualRoot {
    T value;

    T read() const {
        return value;
    }
};

template <typename T>
struct TransitiveVirtualLeft : virtual TransitiveVirtualRoot<T> {};

template <typename T>
struct TransitiveVirtualRight : virtual TransitiveVirtualRoot<T> {};

template <typename T>
struct TransitiveVirtualLeaf : TransitivePadding,
                               TransitiveVirtualLeft<T>,
                               TransitiveVirtualRight<T> {
    T read_virtual_member() const {
        return this->value;
    }

    T call_virtual_base_method() const {
        return this->read();
    }
};

int main() {
    TransitiveLeaf<int> object{};
    TransitiveLeaf<long long> wide_object{};
    TransitiveVirtualLeaf<int> virtual_object{};
    TransitiveVirtualLeaf<long long> virtual_wide_object{};
    object.value = 9;
    wide_object.value = 12;
    virtual_object.value = 21;
    virtual_wide_object.value = 22;
    return object.read() == 9 && object.choose(3) == 12 &&
                   wide_object.read() == 12 && wide_object.choose(7) == 19
                   && virtual_object.read_virtual_member() == 21 &&
                   virtual_object.call_virtual_base_method() == 21 &&
                   virtual_wide_object.read_virtual_member() == 22 &&
                   virtual_wide_object.call_virtual_base_method() == 22
               ? 0
               : 1;
}
