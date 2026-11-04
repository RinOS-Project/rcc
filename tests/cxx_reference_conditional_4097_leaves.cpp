struct ConditionalLargeBranchValue {
    int value;
    explicit ConditionalLargeBranchValue(int initial_value)
        : value(initial_value) {}
    ~ConditionalLargeBranchValue() {}
};

#define RCC_CONDITIONAL_DUP_1(value) (select ? (value) : (value))
#define RCC_CONDITIONAL_DUP_2(value) \
    (select ? RCC_CONDITIONAL_DUP_1(value) : RCC_CONDITIONAL_DUP_1(value))
#define RCC_CONDITIONAL_DUP_3(value) \
    (select ? RCC_CONDITIONAL_DUP_2(value) : RCC_CONDITIONAL_DUP_2(value))
#define RCC_CONDITIONAL_DUP_4(value) \
    (select ? RCC_CONDITIONAL_DUP_3(value) : RCC_CONDITIONAL_DUP_3(value))
#define RCC_CONDITIONAL_DUP_5(value) \
    (select ? RCC_CONDITIONAL_DUP_4(value) : RCC_CONDITIONAL_DUP_4(value))
#define RCC_CONDITIONAL_DUP_6(value) \
    (select ? RCC_CONDITIONAL_DUP_5(value) : RCC_CONDITIONAL_DUP_5(value))
#define RCC_CONDITIONAL_DUP_7(value) \
    (select ? RCC_CONDITIONAL_DUP_6(value) : RCC_CONDITIONAL_DUP_6(value))
#define RCC_CONDITIONAL_DUP_8(value) \
    (select ? RCC_CONDITIONAL_DUP_7(value) : RCC_CONDITIONAL_DUP_7(value))
#define RCC_CONDITIONAL_DUP_9(value) \
    (select ? RCC_CONDITIONAL_DUP_8(value) : RCC_CONDITIONAL_DUP_8(value))
#define RCC_CONDITIONAL_DUP_10(value) \
    (select ? RCC_CONDITIONAL_DUP_9(value) : RCC_CONDITIONAL_DUP_9(value))
#define RCC_CONDITIONAL_DUP_11(value) \
    (select ? RCC_CONDITIONAL_DUP_10(value) : RCC_CONDITIONAL_DUP_10(value))
#define RCC_CONDITIONAL_DUP_12(value) \
    (select ? RCC_CONDITIONAL_DUP_11(value) : RCC_CONDITIONAL_DUP_11(value))

int main(int argc, char**) {
    int select = argc != 0;
    const ConditionalLargeBranchValue&& reference =
        select
        ? static_cast<ConditionalLargeBranchValue&&>(
              ConditionalLargeBranchValue{1})
        : RCC_CONDITIONAL_DUP_12(
              static_cast<ConditionalLargeBranchValue&&>(
                  ConditionalLargeBranchValue{2}));
    return reference.value;
}

#undef RCC_CONDITIONAL_DUP_12
#undef RCC_CONDITIONAL_DUP_11
#undef RCC_CONDITIONAL_DUP_10
#undef RCC_CONDITIONAL_DUP_9
#undef RCC_CONDITIONAL_DUP_8
#undef RCC_CONDITIONAL_DUP_7
#undef RCC_CONDITIONAL_DUP_6
#undef RCC_CONDITIONAL_DUP_5
#undef RCC_CONDITIONAL_DUP_4
#undef RCC_CONDITIONAL_DUP_3
#undef RCC_CONDITIONAL_DUP_2
#undef RCC_CONDITIONAL_DUP_1
