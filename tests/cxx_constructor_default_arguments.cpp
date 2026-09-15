class DefaultBase {
public:
    explicit DefaultBase(int value = 9) : value_(value) {}

    int value() const {
        return value_;
    }

private:
    int value_;
};

class DefaultDerived : public DefaultBase {
public:
    explicit DefaultDerived(int value = 4) : own_(value + 2) {}

    int total() const {
        return value() + own_;
    }

private:
    int own_;
};

extern "C" int cxx_constructor_default_arguments(void) {
    DefaultDerived direct(3);
    DefaultDerived value_initialized{};
    DefaultDerived* heap = new DefaultDerived();
    int result = direct.total() + value_initialized.total() + heap->total();
    delete heap;
    return result == 44 ? 0 : 1;
}
