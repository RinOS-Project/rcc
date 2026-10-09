namespace friend_ns {
class FriendTemplateHost {
    int value;

    template<class T>
    friend int reveal(T& host) {
        return host.value;
    }
};
}

int main() {
    friend_ns::FriendTemplateHost host;
    return friend_ns::reveal(host);
}
