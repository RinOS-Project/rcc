template<class T, int N = 1> requires (N > 0)
int constrained_friend_call(T& value, int replacement);

class ConstrainedFriendHost {
    int value;

    template<class U, int Count> requires (Count > 0)
    friend int constrained_friend_call(U& value, int replacement);
};

template<class V, int Amount> requires (Amount > 0)
int constrained_friend_call(V& value, int replacement) {
    value.value = replacement + Amount;
    return value.value;
}

int main() {
    ConstrainedFriendHost host;
    return constrained_friend_call<ConstrainedFriendHost, 0>(host, 5);
}
