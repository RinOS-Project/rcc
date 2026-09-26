const char rcc_cpp_preprocessor_date[] = __DATE__;
const char rcc_cpp_preprocessor_time[] = __TIME__;

static_assert(sizeof(__DATE__) == 12, "__DATE__ must use the C++ date literal width");
static_assert(sizeof(__TIME__) == 9, "__TIME__ must use the C++ time literal width");

int preprocessor_cpp_date_time_probe() {
    return rcc_cpp_preprocessor_date[0] == '\0' ||
           rcc_cpp_preprocessor_time[0] == '\0';
}
