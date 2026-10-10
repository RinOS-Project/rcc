class FriendFoldConstraintHost;

template<class H, class... T>
requires (((sizeof(T)) + ...) > 0)
int friend_fold_constraint(H& host, int seed, T... values);

class FriendFoldConstraintHost {
private:
    int value = 37;

    template<class X, class... U>
    requires (((sizeof(U)) + ...) > 0)
    friend int friend_fold_constraint(X& host, int seed, U... values);
};

template<class H, class... V>
requires (((sizeof(V)) + ...) > 0)
int friend_fold_constraint(H& host, int seed, V...) {
    return host.value + seed;
}

int main() {
    FriendFoldConstraintHost host;
    return friend_fold_constraint(host, 7, 2, 5L) == 44 ? 0 : 1;
}
