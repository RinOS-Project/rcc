class HiddenDefaultHost {
    template<class A, class B>
    friend int hidden_default(A& host, int replacement);
};

template<class T, class U = int>
int hidden_default(T& host, int replacement) {
    return replacement;
}
