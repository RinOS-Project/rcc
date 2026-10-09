class FriendTemplateHost {
    int value;

    template<class T>
    friend int reveal_declared(T& host, int replacement);
};

template<class U>
int reveal_declared(U& host, int replacement) {
    host.value = replacement;
    return host.value;
}

class Unrelated {
    int value;
};

int main() {
    Unrelated host;
    return reveal_declared(host, 8);
}
