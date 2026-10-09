class FriendDefaultHost {
    template<class T = int>
    friend int friend_default(T value);
};

int main() {
    return 0;
}
