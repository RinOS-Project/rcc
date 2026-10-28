#define RCC_NORETURN_ATTRIBUTE noreturn

#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif

#if __has_c_attribute(RCC_NORETURN_ATTRIBUTE) == 0
#error "noreturn attribute is unavailable"
#endif

#ifdef __RCC__
#if __has_c_attribute(RCC_NORETURN_ATTRIBUTE) != 202311L
#error "noreturn attribute version is incorrect"
#endif

#if __has_c_attribute(_Noreturn) != 202311L
#error "_Noreturn attribute spelling is unavailable"
#endif

#if __has_c_attribute(__noreturn__) != 202311L
#error "alternate noreturn spelling is unavailable"
#endif
#endif

#if __has_c_attribute(unknown_attribute) != 0
#error "unknown C attributes must report unavailable"
#endif

#if __has_c_attribute(rcc::noreturn) != 0
#error "unknown prefixed C attributes must report unavailable"
#endif

[[noreturn]] void c23_noreturn_prefix(void);
void [[_Noreturn]] c23_noreturn_after_type(void);
void c23_noreturn_after_declarator(void) [[noreturn]];
[[noreturn]] void c23_noreturn_group_first(void),
                  c23_noreturn_group_second(void);

int main(void) {
    return 0;
}
