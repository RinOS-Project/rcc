template<typename First, typename Second>
struct PackPair {
    First first;
    Second second;
};

template<typename ValueType, int Tag>
struct PackTagged {
    ValueType value;
};

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

    template<typename U, typename... Rest>
    U echo_first(U first, Rest...) const {
        return first;
    }

    template<typename... Args>
    int argument_count(Args...) const {
        return sizeof...(Args);
    }

    template<typename... Args>
    int count_matching_pairs(PackPair<Args, Args>...) const {
        return sizeof...(Args);
    }

    template<typename... Args>
    int count_tagged(PackTagged<Args, 7>...) const {
        return sizeof...(Args);
    }

    template<int... Values>
    T add_constants() const {
        return (0 + ... + Values);
    }

    T echo_through_member(T other) const {
        return this->template echo<T>(other);
    }

    T echo_through_object(const Box& other, T value) const {
        return other.template echo<T>(value);
    }

    T echo_through_object_deduced(const Box& other, T value) const {
        return other.template echo(value);
    }

    T echo_through_object_pack(const Box& other, T value) const {
        return other.template echo_first(value, value);
    }

    template<typename U>
    static U identity(U other) {
        return other;
    }
};

int main() {
    Box<int> integers{4};
    Box<long> longs{5};
    return integers.value < 10 && integers.add(3L) == 7 &&
                   integers.echo(9L) == 9L &&
                   integers.echo_through_member(13) == 13 &&
                   integers.echo_through_object(integers, 14) == 14 &&
                   integers.echo_through_object_deduced(integers, 15) == 15 &&
                   integers.echo_through_object_pack(integers, 16) == 16 &&
                   integers.echo_first(17) == 17 &&
                   integers.echo_first(18, 2L, 'x') == 18 &&
                   integers.argument_count() == 0 &&
                   integers.argument_count(1, 2L, 'x') == 3 &&
                   integers.count_matching_pairs(
                       PackPair<int, int>{1, 2},
                       PackPair<long, long>{3, 4}) == 2 &&
                   integers.count_tagged(
                       PackTagged<int, 7>{1},
                       PackTagged<long, 7>{2}) == 2 &&
                   integers.add_constants<2, 3>() == 5 &&
                   integers.add_constants<>() == 0 &&
                   integers.identity(12L) == 12L &&
                   integers.add<short>(2) == 6 &&
                   integers.add_constant() == 6 &&
                   integers.add_constant<3>() == 7 &&
                   longs.add_constant() == 7L &&
                   integers.constrained_add(1L) == 5 &&
                   longs.constrained_add(1) == 6L && longs.add(2) == 7L
               ? 0
               : 1;
}
