template<class H, class T>
requires requires(T* candidate) {
    new int(*candidate);
    delete candidate;
}
int new_expression_requirement(H& host, T* value);

class NewExpressionRequirementHost {
    template<class X, class U>
    requires requires(U* probe) {
        new int(*probe);
        delete probe;
    }
    friend int new_expression_requirement(X& host, U* value);
};

template<class V, class W>
requires requires(W* item) {
    new int(*item);
    delete item;
}
int new_expression_requirement(V& host, W* value) {
    return 37;
}

int main() {
    NewExpressionRequirementHost host;
    int value = 5;
    return new_expression_requirement(host, &value) == 37 ? 0 : 1;
}
