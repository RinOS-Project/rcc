class CastKindRequirementHost;

template<class H, class T>
requires requires(T* candidate) {
    static_cast<void*>(candidate);
}
int cast_kind_requirement(H& host, T* value);

class CastKindRequirementHost {
    template<class X, class U>
    requires requires(U* probe) {
        static_cast<void*>(probe);
    }
    friend int cast_kind_requirement(X& host, U* value);

    template<class X, class V>
    requires requires(V* item) {
        reinterpret_cast<void*>(item);
    }
    friend int cast_kind_requirement(X& host, V* value);
};

int main() {
    CastKindRequirementHost host;
    int value = 0;
    return cast_kind_requirement(host, &value);
}
