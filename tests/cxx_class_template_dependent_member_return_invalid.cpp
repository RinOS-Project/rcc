struct NotConvertibleToInt {};

template<typename T>
struct Box {
    template<typename U>
    U echo(U value) const {
        return value;
    }

    int invalid_return(const Box& other, T value) const {
        return other.template echo<T>(value);
    }
};

int main() {
    Box<NotConvertibleToInt> box;
    return box.invalid_return(box, NotConvertibleToInt{});
}
