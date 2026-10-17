template<typename T, typename Bound = int>
concept NoLargerThan = sizeof(T) <= sizeof(Bound);

template<typename T, typename Bound = T>
concept NoLargerThanDefaultType = sizeof(T) <= sizeof(Bound);

template<typename T, int MinimumSize = sizeof(T),
         int MaximumSize = MinimumSize>
concept FitsMaximumSize = sizeof(T) <= MaximumSize;

template<typename T>
struct Holder {
    T value;
};

template<typename T, template<typename> class Box = Holder>
concept FitsInBox = sizeof(Box<T>) >= sizeof(T);

struct SmallResult {
    int value() const { return 1; }
};

struct LargeResult {
    char value[8];
};

struct SmallProvider {
    int value() const { return 1; }
};

struct LargeProvider {
    LargeResult value() const { return {}; }
};

template<typename T>
concept HasIntSizedValue = requires(T candidate) {
    { candidate.value() } -> NoLargerThan<>;
};

template<typename T>
concept HasSelfSizedValue = requires(T candidate) {
    { candidate.value() } -> FitsMaximumSize<>;
};

template<typename T>
concept HasDefaultTypeBound = requires(T candidate) {
    { candidate.value() } -> NoLargerThanDefaultType<>;
};

template<typename T>
concept HasTemplateBoxValue = requires(T candidate) {
    { candidate.value() } -> FitsInBox<Holder>;
};

template<typename T>
requires HasIntSizedValue<T>
int accepts_int_sized_value(T) {
    return 1;
}

template<typename T>
requires (!HasIntSizedValue<T>)
int accepts_int_sized_value(T) {
    return 0;
}

template<typename T>
requires HasSelfSizedValue<T>
int accepts_self_sized_value(T) {
    return 1;
}

template<typename T>
requires HasDefaultTypeBound<T>
int accepts_dependent_default_type(T) {
    return 1;
}

template<typename T>
requires FitsMaximumSize<T>
int accepts_self_sized_type() {
    return 1;
}

template<typename T>
requires FitsInBox<T>
int accepts_default_template_argument() {
    return 1;
}

template<typename T>
requires HasTemplateBoxValue<T>
int accepts_template_box_value(T) {
    return 1;
}

int accepts_abbreviated_template_argument(FitsInBox auto value) {
    return sizeof(value) != 0;
}

template<FitsInBox T>
int accepts_template_constrained_type(T) {
    return 1;
}

template<typename T>
requires NoLargerThan<T>
int accepts_default_bound(T) {
    return 1;
}

template<typename T>
requires (!NoLargerThan<T>)
int accepts_default_bound(T) {
    return 0;
}

int accepts_abbreviated(NoLargerThan auto value) {
    return sizeof(value) != 0;
}

template<NoLargerThan T>
int accepts_constrained_type_parameter(T) {
    return 1;
}

template<typename T>
struct constrained_box {
    T value;
};

int main() {
    return accepts_int_sized_value(SmallProvider{}) == 1 &&
                   accepts_int_sized_value(LargeProvider{}) == 0 &&
                   accepts_self_sized_value(SmallProvider{}) == 1 &&
                   accepts_self_sized_value(LargeProvider{}) == 1 &&
                   accepts_dependent_default_type(SmallProvider{}) == 1 &&
                   accepts_dependent_default_type(LargeProvider{}) == 1 &&
                   accepts_default_template_argument<int>() == 1 &&
                   accepts_abbreviated_template_argument(1) == 1 &&
                   accepts_template_constrained_type(1) == 1 &&
                   accepts_template_box_value(SmallProvider{}) == 1 &&
                   accepts_self_sized_type<LargeResult>() == 1 &&
                   accepts_default_bound(1) == 1 &&
                   accepts_abbreviated(1) == 1 &&
                   accepts_constrained_type_parameter(1) == 1 &&
                   constrained_box<int>{1}.value == 1
               ? 0
               : 1;
}
