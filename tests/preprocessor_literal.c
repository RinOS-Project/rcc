#define TOKEN 123
#define STRING_PARAMETER(value) "value"

static const char object_macro_name[] = "TOKEN";
static const char function_parameter_name[] = STRING_PARAMETER(replaced);

_Static_assert(sizeof("TOKEN") == 6,
               "object macro expanded inside a string literal");
_Static_assert(sizeof(STRING_PARAMETER(replaced)) == 6,
               "function macro parameter expanded inside a string literal");
