template<typename T>
concept HasFirstTag = requires { typename T::first_tag; };

template<typename T>
concept HasSecondTag = requires { typename T::second_tag; };

template<typename First, typename Second>
struct CrossedConstraintPack {
    static constexpr int kind = 0;
};

template<typename T, typename U> requires HasFirstTag<T>
struct CrossedConstraintPack<T*, U> {
    static constexpr int kind = 1;
};

template<typename T, typename U> requires HasSecondTag<U>
struct CrossedConstraintPack<T, U*> {
    static constexpr int kind = 2;
};

struct BothTags {
    using first_tag = int;
    using second_tag = int;
};

static_assert(CrossedConstraintPack<BothTags*, BothTags*>::kind != 0);
