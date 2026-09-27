#if defined(EXPECT_NO_SSE)
# ifdef __SSE__
#  error __SSE__ must be absent with -mno-sse
# endif
#else
# ifndef __SSE__
#  error __SSE__ must be defined by the default RinOS target profile
# endif
#endif

#if defined(EXPECT_NO_SSE2)
# ifdef __SSE2__
#  error __SSE2__ must be absent with -mno-sse2
# endif
#else
# ifndef __SSE2__
#  error __SSE2__ must be defined by the default RinOS target profile
# endif
#endif

int main(void) { return 0; }
