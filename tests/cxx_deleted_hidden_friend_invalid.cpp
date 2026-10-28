struct ExplicitlyDeletedFriend {
    friend int deleted_friend_call(ExplicitlyDeletedFriend) = delete;
};

int main() {
    ExplicitlyDeletedFriend value{};
    return deleted_friend_call(value);
}
