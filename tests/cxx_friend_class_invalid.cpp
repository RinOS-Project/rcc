namespace api {

class Secret {
    int value;

public:
    explicit Secret(int initial) : value(initial) {}
    friend class Reader;
};

class Other {
public:
    int read(const Secret& secret) const {
        return secret.value;
    }
};

}
