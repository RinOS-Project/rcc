#define RCC_CONTINUED_VALUE(value) ((value) + \
                                    2)

#if defined(RCC_CONTINUATION_LEFT) && \
    defined(RCC_CONTINUATION_RIGHT)
int rcc_continued_condition = RCC_CONTINUED_VALUE(40);
#else
#error continued #if expression was not evaluated as one logical line
#endif

int rcc_joined_identi\
fier = 42;
