class DuplicateFriendTemplateHost {
    int value;

    template<class T>
    friend int duplicate_reveal(T& host) {
        return host.value;
    }
};

template<class U>
int duplicate_reveal(U& host) {
    return host.value;
}

int main() {
    DuplicateFriendTemplateHost host;
    return duplicate_reveal(host);
}
