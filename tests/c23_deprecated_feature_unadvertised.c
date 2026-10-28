#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif

#if __has_c_attribute(deprecated) != 0
#error "RCC must not advertise incomplete C deprecated-attribute support"
#endif

int c23_deprecated_feature_unadvertised(void) {
    return 0;
}
