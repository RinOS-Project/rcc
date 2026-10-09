class FriendHost {
    int value;

public:
    friend int inline_friend(FriendHost& host, int replacement) {
        host.value = replacement;
        return host.value + 1;
    }

    friend int declared_friend(FriendHost& host, int replacement);
};

int declared_friend(FriendHost& host, int replacement) {
    host.value += replacement;
    return host.value + 2;
}

class FriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host, int replacement) {
        host.value = replacement;
        return host.value;
    }

    template<class T>
    friend int reveal_declared(T& host, int replacement);
};

class SecondFriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host, int replacement);
};

template<class U>
int reveal_declared(U& host, int replacement) {
    host.value = replacement;
    return host.value;
}

int main() {
    FriendHost ordinary;
    FriendTemplateHost host;
    SecondFriendTemplateHost second_host;
    int ordinary_result = inline_friend(ordinary, 4) +
                          declared_friend(ordinary, 5);
    return ordinary_result == 16 && reveal(host, 42) == 42 &&
           reveal_declared(host, 37) == 37 &&
           reveal(second_host, 19) == 19 ? 0 : 1;
}
