template<class H, class T>
requires requires(T* candidate) {
    new int(*candidate);
}
int new_expression_kind_mismatch(H& host, T* value);

class NewExpressionKindMismatchHost {
    template<class X, class U>
    requires requires(U* probe) {
        new int(*probe);
    }
    friend int new_expression_kind_mismatch(X& host, U* value);

    template<class X, class U>
    requires requires(U* item) {
        new int{*item};
    }
    friend int new_expression_kind_mismatch(X& host, U* value);
};

int main() {
    NewExpressionKindMismatchHost host;
    int value = 5;
    return new_expression_kind_mismatch(host, &value);
}
