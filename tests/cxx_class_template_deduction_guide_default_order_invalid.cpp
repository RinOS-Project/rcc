template <typename T>
struct InvalidDefaultGuideBox {
    T value;
};

template <typename T>
InvalidDefaultGuideBox(int = 0, T) -> InvalidDefaultGuideBox<T>;
