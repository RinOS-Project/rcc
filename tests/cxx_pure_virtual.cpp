class AbstractValue {
public:
    virtual int value() = 0;
};

class ConcreteValue : public AbstractValue {
public:
    int value() override { return 42; }
};

int main() {
    ConcreteValue value{};
    AbstractValue* interface = &value;
    return interface->value() == 42 ? 0 : 1;
}
