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

#define RCC_COMMENT_FLAG 8 /* replacement-list comment */
/* RCC_COMMENT_FLAG must not expand inside this comment. */
int rcc_comment_replacement = RCC_COMMENT_FLAG;

/*
#error a directive inside a block comment must remain inactive
*/

const char* rcc_comment_markers_in_string = "/* not a comment */";

_Static_assert(sizeof("Rin" "OS") == 6u,
               "adjacent string literals must concatenate");
int rcc_constant_expression_array[sizeof(unsigned int)];
