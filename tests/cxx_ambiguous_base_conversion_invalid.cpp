class Base {
public:
    int value;
};

class Left : public Base {};
class Right : public Base {};
class Diamond : public Left, public Right {};

void take_base(Base& value);
Base* take_base_pointer(Base* value);

int reject_ambiguous_reference(Diamond& value) {
    take_base(value);
    return value.value;
}

Base* reject_ambiguous_pointer(Diamond* value) {
    return take_base_pointer(value);
}
