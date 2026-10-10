template<class H, class T>
requires requires(T* candidate) {
    delete candidate;
}
int delete_expression_kind_mismatch(H& host, T* value);

class DeleteExpressionKindMismatchHost {
    template<class X, class U>
    requires requires(U* probe) {
        delete probe;
    }
    friend int delete_expression_kind_mismatch(X& host, U* value);

    template<class X, class U>
    requires requires(U* item) {
        delete[] item;
    }
    friend int delete_expression_kind_mismatch(X& host, U* value);
};

int main() {
    DeleteExpressionKindMismatchHost host;
    int value = 5;
    return delete_expression_kind_mismatch(host, &value);
}
