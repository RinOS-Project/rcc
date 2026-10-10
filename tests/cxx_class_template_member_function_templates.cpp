template<typename T>
struct Box {
    T value;

    template<typename U>
    T add(U other) const {
        return value + static_cast<T>(other);
    }

    template<int Amount = 2>
    T add_constant() const {
        return value + Amount;
    }

    template<typename U>
        requires (sizeof(T) > 0)
    T constrained_add(U other) const {
        return value + static_cast<T>(other);
    }

    template<typename U>
    U echo(U other) const {
        return other;
    }

    template<typename U>
    static U identity(U other) {
        return other;
    }
};

int main() {
    Box<int> integers{4};
    Box<long> longs{5};
    return integers.add(3L) == 7 && integers.echo(9L) == 9L &&
                   integers.identity(12L) == 12L &&
                   integers.add_constant() == 6 &&
                   longs.add_constant() == 7L &&
                   integers.constrained_add(1L) == 5 &&
                   longs.constrained_add(1) == 6L && longs.add(2) == 7L
               ? 0
               : 1;
}
