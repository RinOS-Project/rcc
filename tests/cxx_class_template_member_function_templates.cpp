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

    template<int Tag, typename... Args>
    int count_tagged(PackTagged<Args, Tag>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_after(PackTagged<Args, (Tag + 1)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_divided(PackTagged<Args, (Tag / 2)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_remainder(PackTagged<Args, (Tag % 4)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_and(PackTagged<Args, (Tag & 3)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_or(PackTagged<Args, (Tag | 1)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_xor(PackTagged<Args, (Tag ^ 3)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_left_shift(PackTagged<Args, (Tag << 1)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_right_shift(PackTagged<Args, (Tag >> 1)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_logical_not(PackTagged<Args, (!Tag)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_complement(PackTagged<Args, (~Tag)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_equal(PackTagged<Args, (Tag == 6)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_logical_and(
        PackTagged<Args, ((Tag == 6) && (Tag != 0))>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_logical_or(
        PackTagged<Args, ((Tag > 8) || (Tag <= 6))>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_select(
        PackTagged<Args, (Tag < 7 ? (Tag >= 6) : (Tag <= 3))>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_cast(PackTagged<Args, static_cast<int>(Tag)>...) const {
        return sizeof...(Args);
    }

    template<int Tag, typename... Args>
    int count_tagged_c_style_cast(PackTagged<Args, (int)Tag>...) const {
        return sizeof...(Args);
    }

    template<typename... Args>
    int count_tagged_pack_size(PackTagged<Args, sizeof...(Args)>...) const {
        return sizeof...(Args);
    }

    template<typename... Args>
    int count_tagged_type_size(PackTagged<Args, sizeof(Args)>...) const {
        return sizeof...(Args);
    }

    template<int... Values>
    T add_constants() const {
        return (0 + ... + Values);
    }

    template<int... Values>
    int count_expanded_member_arguments() const {
        return this->template count_values<Values...>();
    }

    template<int... Values>
    int count_values() const {
        return sizeof...(Values);
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
                   integers.count_tagged_after<6>(
                       PackTagged<int, 7>{1},
                       PackTagged<long, 7>{2}) == 2 &&
                   integers.count_tagged_divided<6>(
                       PackTagged<int, 3>{1},
                       PackTagged<long, 3>{2}) == 2 &&
                   integers.count_tagged_remainder<6>(
                       PackTagged<int, 2>{1},
                       PackTagged<long, 2>{2}) == 2 &&
                   integers.count_tagged_and<6>(
                       PackTagged<int, 2>{1},
                       PackTagged<long, 2>{2}) == 2 &&
                   integers.count_tagged_or<6>(
                       PackTagged<int, 7>{1},
                       PackTagged<long, 7>{2}) == 2 &&
                   integers.count_tagged_xor<6>(
                       PackTagged<int, 5>{1},
                       PackTagged<long, 5>{2}) == 2 &&
                   integers.count_tagged_left_shift<3>(
                       PackTagged<int, 6>{1},
                       PackTagged<long, 6>{2}) == 2 &&
                   integers.count_tagged_right_shift<12>(
                       PackTagged<int, 6>{1},
                       PackTagged<long, 6>{2}) == 2 &&
                   integers.count_tagged_logical_not<0>(
                       PackTagged<int, 1>{1},
                       PackTagged<long, 1>{2}) == 2 &&
                   integers.count_tagged_complement<6>(
                       PackTagged<int, -7>{1},
                       PackTagged<long, -7>{2}) == 2 &&
                   integers.count_tagged_equal<6>(
                       PackTagged<int, 1>{1},
                       PackTagged<long, 1>{2}) == 2 &&
                   integers.count_tagged_logical_and<6>(
                       PackTagged<int, 1>{1},
                       PackTagged<long, 1>{2}) == 2 &&
                   integers.count_tagged_logical_or<6>(
                       PackTagged<int, 1>{1},
                       PackTagged<long, 1>{2}) == 2 &&
                   integers.count_tagged_select<6>(
                       PackTagged<int, 1>{1},
                       PackTagged<long, 1>{2}) == 2 &&
                   integers.count_tagged_cast<6>(
                       PackTagged<int, 6>{1},
                       PackTagged<long, 6>{2}) == 2 &&
                   integers.count_tagged_c_style_cast<6>(
                       PackTagged<int, 6>{1},
                       PackTagged<long, 6>{2}) == 2 &&
                   integers.count_tagged_pack_size(
                       PackTagged<int, 2>{1},
                       PackTagged<long, 2>{2}) == 2 &&
                   integers.count_tagged_pack_size() == 0 &&
                   integers.count_tagged_type_size(
                       PackTagged<int, 4>{1},
                       PackTagged<long long, 8>{2},
                       PackTagged<char, 1>{3}) == 3 &&
                   integers.count_tagged_type_size() == 0 &&
                   integers.add_constants<2, 3>() == 5 &&
                   integers.add_constants<>() == 0 &&
                   integers.count_expanded_member_arguments<
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                       12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                       23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33>() == 33 &&
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
