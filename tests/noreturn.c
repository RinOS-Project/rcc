/* C17 _Noreturn is a function specifier, not a replacement type. */
_Noreturn void declared_noreturn(void);
void _Noreturn defined_noreturn(void) {
    for (;;) {
    }
}

int call_noreturn(void) {
    declared_noreturn();
    return 0;
}
