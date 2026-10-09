template<class T = int>
int redeclared_default(T value);

class DefaultFriendHost {
    template<class U>
    friend int redeclared_default(U value);
};

template<class V = int>
int redeclared_default(V value) {
    return value;
}
