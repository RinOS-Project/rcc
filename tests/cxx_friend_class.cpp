namespace api {

class Secret {
    int value;

    int hidden() const {
        return value + 1;
    }

    friend class Reader;

public:
    explicit Secret(int initial) : value(initial) {}
};

class Reader {
public:
    int read(const Secret& secret) const {
        return secret.value + secret.hidden();
    }
};

}

int main() {
    api::Secret secret(20);
    api::Reader reader{};
    return reader.read(secret) == 41 ? 0 : 1;
}
