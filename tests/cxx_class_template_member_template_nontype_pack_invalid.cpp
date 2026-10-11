template<typename ValueType, int Tag>
struct PackTagged {
    ValueType value;
};

struct Box {
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
};

int main() {
    Box box;
    return box.count_tagged(
        PackTagged<int, 7>{1},
        PackTagged<long, 8>{2}) +
        box.count_tagged_after<6>(PackTagged<int, 8>{1}) +
        box.count_tagged_divided<6>(PackTagged<int, 4>{1}) +
        box.count_tagged_remainder<6>(PackTagged<int, 3>{1}) +
        box.count_tagged_and<6>(PackTagged<int, 3>{1}) +
        box.count_tagged_or<6>(PackTagged<int, 6>{1}) +
        box.count_tagged_xor<6>(PackTagged<int, 4>{1}) +
        box.count_tagged_left_shift<3>(PackTagged<int, 7>{1}) +
        box.count_tagged_right_shift<12>(PackTagged<int, 5>{1}) +
        box.count_tagged_logical_not<0>(PackTagged<int, 2>{1}) +
        box.count_tagged_complement<6>(PackTagged<int, 6>{1}) +
        box.count_tagged_equal<6>(PackTagged<int, 0>{1}) +
        box.count_tagged_logical_and<6>(PackTagged<int, 0>{1}) +
        box.count_tagged_logical_or<6>(PackTagged<int, 0>{1}) +
        box.count_tagged_select<6>(PackTagged<int, 0>{1}) +
        box.count_tagged_cast<6>(PackTagged<int, 5>{1});
}
