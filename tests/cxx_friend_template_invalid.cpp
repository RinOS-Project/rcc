class FriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host, int replacement) {
        host.value = replacement;
        return host.value;
    }

};

class Unrelated {
    int value;
};

int main() {
    Unrelated host;
    return reveal(host, 7);
}
