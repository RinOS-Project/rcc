template<class T, int N = 3>
int duplicate_non_type_default(T value);

class FriendNonTypeDefaultHost {
    template<class U, int Count>
    friend int duplicate_non_type_default(U value);
};

template<class V, int Amount = 4>
int duplicate_non_type_default(V value) {
    return value + Amount;
}
