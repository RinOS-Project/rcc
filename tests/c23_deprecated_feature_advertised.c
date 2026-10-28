#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif

#if __has_c_attribute(deprecated) != 202311L
#error "RCC must advertise C23 deprecated-attribute support"
#endif

int c23_deprecated_feature_advertised(void) {
    return 0;
}
