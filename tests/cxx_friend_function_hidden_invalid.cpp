class OrdinaryHiddenHost {
    int value;

    friend int ordinary_hidden(OrdinaryHiddenHost& host) {
        return host.value;
    }
};

class Unrelated {};

int main() {
    Unrelated value;
    return ordinary_hidden(value);
}
