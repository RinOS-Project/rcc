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

class ImplicitDefaultMemberInitializer {
public:
    int total() const {
        return first_ + second_;
    }

private:
    int first_ = 6;
    int second_ = 4;
};

extern "C" int cxx_default_member_initializer() {
    DefaultMemberInitializer explicit_value(4);
    DefaultMemberInitializer default_value{};
    DefaultMemberInitializer* heap = new DefaultMemberInitializer{};
    ImplicitDefaultMemberInitializer* implicit =
        new ImplicitDefaultMemberInitializer{};
    ImplicitDefaultMemberInitializer* implicit_paren =
        new ImplicitDefaultMemberInitializer();
    ImplicitDefaultMemberInitializer* implicit_plain =
        new ImplicitDefaultMemberInitializer;
    ImplicitDefaultMemberInitializer* implicit_array =
        new ImplicitDefaultMemberInitializer[2];
    ImplicitDefaultMemberInitializer* implicit_value_array =
        new ImplicitDefaultMemberInitializer[2]{};
    ImplicitDefaultMemberInitializer* implicit_paren_array =
        new ImplicitDefaultMemberInitializer[2]();
    int result = explicit_value.total() + default_value.total() +
                 heap->total() + implicit->total() + implicit_paren->total() +
                 implicit_plain->total() + implicit_array[0].total() +
                 implicit_array[1].total() +
                 implicit_value_array[0].total() +
                 implicit_value_array[1].total() +
                 implicit_paren_array[0].total() +
                 implicit_paren_array[1].total();
    delete heap;
    delete implicit;
    delete implicit_paren;
    delete implicit_plain;
    delete[] implicit_array;
    delete[] implicit_value_array;
    delete[] implicit_paren_array;
    return result == 138 ? 0 : 1;
}
