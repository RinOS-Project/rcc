class AbstractValue {
public:
    virtual int value() = 0;
};

int main() {
    AbstractValue* allocated = new AbstractValue;
    return allocated != 0;
}
