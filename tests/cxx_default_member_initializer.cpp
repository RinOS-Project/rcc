class DefaultMemberInitializer {
public:
    explicit DefaultMemberInitializer(int value) : override_(value) {}

    DefaultMemberInitializer() : extra_(2) {
        override_ += 1;
    }

    int total() const {
        return inherited_ + override_ + extra_;
    }

private:
    int inherited_ = 5;
    int override_ = 7;
    int extra_ = 9;
};

extern "C" int cxx_default_member_initializer() {
    DefaultMemberInitializer explicit_value(4);
    DefaultMemberInitializer default_value{};
    DefaultMemberInitializer* heap = new DefaultMemberInitializer{};
    int result = explicit_value.total() + default_value.total() +
                 heap->total();
    delete heap;
    return result == 48 ? 0 : 1;
}
