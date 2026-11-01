struct ExternalStaticDataApi {
    static int value;
};

struct LegacyConstStaticDataApi {
#if __cplusplus >= 201703L
    static const int value = 37;
#else
    static constexpr int value = 37;
#endif
};

#if __cplusplus >= 201703L
struct InlineConstexprStaticDataApi {
    static constexpr int value = 43;
};
#endif

#if defined(TEST_EXTERNAL_STATIC_DATA_DEFINITIONS)
int ExternalStaticDataApi::value = 41;
#if __cplusplus >= 201703L
const int LegacyConstStaticDataApi::value;
#else
constexpr int LegacyConstStaticDataApi::value;
#endif
#elif defined(TEST_EXTERNAL_STATIC_DATA_MAIN)
extern "C" int read_external_static_data();

int main() {
#if __cplusplus >= 201703L
    return read_external_static_data() == 121 ? 0 : 1;
#else
    return read_external_static_data() == 78 ? 0 : 1;
#endif
}
#else
extern "C" int read_external_static_data() {
    int result = ExternalStaticDataApi::value +
                 *&LegacyConstStaticDataApi::value;
#if __cplusplus >= 201703L
    result += *&InlineConstexprStaticDataApi::value;
#endif
    return result;
}
#endif
