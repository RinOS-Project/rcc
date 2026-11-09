#if defined(RCC_TEST_ELIFDEF_MISSING_NAME)
#if 0
#elifdef
#endif
#elif defined(RCC_TEST_ELIFDEF_TRAILING)
#if 0
#elifdef PRESENT trailing
#endif
#elif defined(RCC_TEST_ELIFDEF_AFTER_ELSE)
#if 0
#else
#elifdef PRESENT
#endif
#else
#error "select one malformed-directive case"
#endif
