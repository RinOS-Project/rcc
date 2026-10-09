template <typename T>
struct RepeatedTransitiveRoot {
    T value;

    int read() const {
        return 1;
    }
};

template <typename T>
struct RepeatedTransitiveLeft : RepeatedTransitiveRoot<T> {};

template <typename T>
struct RepeatedTransitiveRight : RepeatedTransitiveRoot<T> {};

template <typename T>
struct RepeatedTransitiveLeaf : RepeatedTransitiveLeft<T>,
                                RepeatedTransitiveRight<T> {
    T read_field() const {
        return this->value;
    }

    int call_method() const {
        return this->read();
    }
};

int main() {
    RepeatedTransitiveLeaf<int> object{};
    return object.read_field() + object.call_method();
}
