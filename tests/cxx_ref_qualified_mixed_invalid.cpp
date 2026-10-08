struct InvalidRefQualifierMix {
    int select() const {
        return 1;
    }

    int select() && {
        return 2;
    }
};

struct RefQualifiedOverrideBase {
    virtual int value() & {
        return 1;
    }
};

struct RefQualifiedOverrideMismatch : RefQualifiedOverrideBase {
    int value() && override {
        return 2;
    }
};

struct InvalidStaticRefQualifier {
    static int value() && {
        return 3;
    }
};

struct InvalidConstructorRefQualifier {
    InvalidConstructorRefQualifier() & {}
};

struct InvalidFriendRefQualifier {
    friend int friend_value() & {
        return 4;
    }
};
