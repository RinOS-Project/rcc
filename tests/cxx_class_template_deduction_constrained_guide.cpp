template <typename Value>
struct ConstrainedGuideBox {
    long long value;
};

template <typename T>
ConstrainedGuideBox(T) -> ConstrainedGuideBox<short>;

template <typename T>
    requires(sizeof(T) == 8)
ConstrainedGuideBox(T) -> ConstrainedGuideBox<char>;

template <typename T>
    requires(sizeof(T) == 4)
ConstrainedGuideBox(T) -> ConstrainedGuideBox<long>;

int main() {
    ConstrainedGuideBox selected{17};
    ConstrainedGuideBox<long>* exact = &selected;
    long long wide_value = 29;
    ConstrainedGuideBox wide{wide_value};
    ConstrainedGuideBox<char>* exact_wide = &wide;
    return exact->value == 17 && exact_wide->value == 29 ? 0 : 1;
}
