class ProtectedBase {
protected:
    static int counter;

    static int read_counter() {
        return counter;
    }
};

int ProtectedBase::counter = 41;

class ProtectedDerived : public ProtectedBase {
public:
    int expose() {
        return ProtectedBase::counter + ProtectedBase::read_counter();
    }
};

int main() {
    ProtectedDerived value;
    return value.expose() == 82 ? 0 : 1;
}
