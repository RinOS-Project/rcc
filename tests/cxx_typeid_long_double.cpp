#ifndef __RCC__
#include <typeinfo>
#endif

template <class T> struct TypeIdFloatingBox { T value; };

extern "C" int main() {
    return &typeid(TypeIdFloatingBox<double>) !=
            &typeid(TypeIdFloatingBox<long double>)
        ? 0 : 1;
}
