typedef unsigned short char16_t;
typedef unsigned int char32_t;

#ifdef RCC_TEST_WIDE_MULTICHAR_VALUE
_Static_assert(u'AB' == 'A', "RinOS keeps the first UTF-16 code unit");
_Static_assert(U'AB' == 'A', "RinOS keeps the first UTF-32 code unit");
#endif

char16_t multi_utf16 = u'AB';
char32_t multi_utf32 = U'AB';
