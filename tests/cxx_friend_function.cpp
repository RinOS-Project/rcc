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

int main() {
    return inline_friend(4) + declared_friend(5) == 12 ? 0 : 1;
}
