class virtual_base {
public:
    virtual int value() { return 1; }
};

class virtual_derived : public virtual_base {
public:
    int value() override { return 2; }
};

virtual_base global_base;
virtual_derived global_derived;

int main() {
    virtual_derived derived{};
    virtual_base base{};
    virtual_base* pointer = &derived;
    virtual_base* global_pointer = &global_derived;
    return base.value() == 1 && pointer->value() == 2 &&
                   global_base.value() == 1 &&
                   global_pointer->value() == 2 ? 0 : 1;
}
