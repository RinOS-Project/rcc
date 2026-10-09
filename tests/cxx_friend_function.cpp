class FriendHost {
public:
    friend int inline_friend(int value) {
        return value + 1;
    }

    friend int declared_friend(int value);
};

int declared_friend(int value) {
    return value + 2;
}

class FriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host, int replacement) {
        host.value = replacement;
        return host.value;
    }
};

int main() {
    FriendTemplateHost host;
    return inline_friend(4) + declared_friend(5) == 12 &&
           reveal(host, 42) == 42 ? 0 : 1;
}
