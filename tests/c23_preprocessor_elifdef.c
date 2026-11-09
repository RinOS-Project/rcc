#define RCC_PP_PRESENT 1

#if 0
#error "#elifdef must not override a skipped first branch"
#elifdef RCC_PP_PRESENT
#define RCC_PP_SELECTED 41
#elifndef RCC_PP_PRESENT
#error "#elifndef must be false for a defined macro"
#else
#error "the selected #elifdef branch must suppress later alternatives"
#endif

#if 1
#define RCC_PP_FIRST_BRANCH 1
#elifdef RCC_PP_PRESENT
#error "#elifdef must not replace a prior true branch"
#elifndef RCC_PP_MISSING
#error "#elifndef must not replace a prior true branch"
#else
#error "#else must not replace a prior true branch"
#endif

#if 0
#if 0
#elifdef RCC_PP_PRESENT
#error "an inactive parent must keep nested #elifdef inactive"
#elifndef RCC_PP_MISSING
#error "an inactive parent must keep nested #elifndef inactive"
#else
#error "an inactive parent must keep nested #else inactive"
#endif
#endif

#if 0
#elifndef RCC_PP_MISSING
#define RCC_PP_UNDEFINED_SELECTED 1
#else
#error "#elifndef must select an undefined identifier"
#endif

#if !defined(RCC_PP_SELECTED) || RCC_PP_SELECTED != 41
#error "defined #elifdef branch was not selected"
#endif
#if !defined(RCC_PP_FIRST_BRANCH) || !defined(RCC_PP_UNDEFINED_SELECTED)
#error "conditional branch state was not preserved"
#endif

int c23_preprocessor_elifdef(void) {
    return RCC_PP_SELECTED == 41 ? 0 : 1;
}
