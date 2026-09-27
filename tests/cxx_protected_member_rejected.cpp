class ProtectedBaseRejected {
protected:
    static int counter;
};

int ProtectedBaseRejected::counter = 7;

class UnrelatedProtectedAccess {
public:
    int expose() {
        return ProtectedBaseRejected::counter;
    }
};

int main() {
    UnrelatedProtectedAccess value;
    return value.expose();
}
