class BaseConstructor {
public:
    explicit BaseConstructor(int value) : base_(value) {}

    int base() const {
        return base_;
    }

private:
    int base_;
};

class DerivedConstructor : public BaseConstructor {
public:
    explicit DerivedConstructor(int value)
        : BaseConstructor(value + 1), own_(value + 2) {}

    int total() const {
        return base() + own_;
    }

private:
    int own_;
};

extern "C" int cxx_base_constructor_initializer() {
    DerivedConstructor direct(4);
    DerivedConstructor* heap = new DerivedConstructor(6);
    int result = direct.total() + heap->total();
    delete heap;
    return result == 26 ? 0 : 1;
}
