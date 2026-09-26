class AbstractValue {
public:
    virtual int value() = 0;
};

int main() {
    AbstractValue value{};
    AbstractValue* allocated = new AbstractValue;
    return value.value() + (allocated != 0);
}
