/* An invalid preprocessing constant expression must be diagnosed. */
#if (1 + )
#endif

int invalid_preprocessor_if;
