#if __has_builtin(__is_function)
typedef void IsFunctionBuiltinFunction();
typedef int IsFunctionBuiltinVariadic(double, ...);
typedef void (*IsFunctionBuiltinPointer)();

static_assert(__is_function(IsFunctionBuiltinFunction),
              "function type recognized");
static_assert(__is_function(IsFunctionBuiltinVariadic),
              "variadic function recognized");
static_assert(!__is_function(IsFunctionBuiltinPointer),
              "function pointer is not a function type");

template<typename T>
struct IsFunctionBuiltinProbe {
    static constexpr bool value = __is_function(T);
};

static_assert(IsFunctionBuiltinProbe<IsFunctionBuiltinFunction>::value,
              "dependent function type recognized after substitution");
static_assert(!IsFunctionBuiltinProbe<IsFunctionBuiltinPointer>::value,
              "dependent function pointer rejected after substitution");

template<typename T>
struct FunctionTypePackProbe {
    static constexpr int value = 0;
};

template<typename R, typename... Args>
struct FunctionTypePackProbe<R(Args...)> {
    static constexpr int value = 1;
};

template<typename R, typename... Args>
struct FunctionTypePackProbe<R(Args..., ...)> {
    static constexpr int value = 2;
};

typedef int FixedFunctionType(double, char);
typedef int VariadicFunctionType(double, char, ...);
static_assert(FunctionTypePackProbe<FixedFunctionType>::value == 1,
              "function type parameter packs match fixed signatures");
static_assert(FunctionTypePackProbe<VariadicFunctionType>::value == 2,
              "function type parameter packs match variadic signatures");

template<typename T>
struct FunctionTypeSuffixPackProbe {
    static constexpr int value = 0;
};

template<typename R, typename... Args>
struct FunctionTypeSuffixPackProbe<R(Args..., int)> {
    static constexpr int value = 1;
};

typedef int FunctionTypeWithSuffix(double, char, int);
static_assert(FunctionTypeSuffixPackProbe<FunctionTypeWithSuffix>::value == 1,
              "function type packs may precede fixed suffix parameters");
#else
static_assert(!__has_builtin(__is_function),
              "unsupported compilers do not advertise the RCC extension");
#endif
