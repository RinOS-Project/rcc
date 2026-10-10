#ifndef __RCC__
#include <typeinfo>
#endif

template<class T>
requires requires(T candidate, int index) {
    candidate[index];
    candidate = candidate;
    (candidate, index);
    noexcept(candidate[index]);
    typeid(candidate);
    "friend requirement literal";
}
int friend_expression_forms(T& host, int index);

class FriendExpressionFormsHost {
public:
    int operator[](int index) const noexcept { return 11 + index; }

private:
    int value = 31;

    template<class U>
    requires requires(U probe, int position) {
        probe[position];
        probe = probe;
        (probe, position);
        noexcept(probe[position]);
        typeid(probe);
        "friend requirement literal";
    }
    friend int friend_expression_forms(U& host, int index);
};

template<class V>
requires requires(V item, int offset) {
    item[offset];
    item = item;
    (item, offset);
    noexcept(item[offset]);
    typeid(item);
    "friend requirement literal";
}
int friend_expression_forms(V& host, int index) {
    return host.value + host[index];
}

int main() {
    FriendExpressionFormsHost host;
    return friend_expression_forms(host, 2) == 44 ? 0 : 1;
}
