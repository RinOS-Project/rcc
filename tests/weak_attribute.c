__attribute__((weak)) int rcc_weak_attribute_function(void) {
    return 17;
}

int rcc_weak_attribute_data __attribute__((weak)) = 29;

extern int rcc_weak_attribute_import __attribute__((weak));

int rcc_weak_attribute_read(void) {
    return rcc_weak_attribute_import;
}
