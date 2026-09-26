#if !__has_include("hello.cpp")
#error "__has_include failed to find a relative source header"
#endif

#if __has_include(<rcc-header-that-does-not-exist.hpp>)
#error "__has_include reported a missing system header"
#endif

int preprocessor_has_include_cxx() {
    return 0;
}
