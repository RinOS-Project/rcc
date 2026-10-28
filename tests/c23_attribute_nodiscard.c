#define RCC_NODISCARD_ATTRIBUTE nodiscard

#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif

#if __has_c_attribute(RCC_NODISCARD_ATTRIBUTE) == 0
#error "nodiscard attribute is unavailable"
#endif

#ifdef __RCC__
#if __has_c_attribute(RCC_NODISCARD_ATTRIBUTE) != 202311L
#error "nodiscard attribute version is incorrect"
#endif
#endif

#if __has_c_attribute(unknown_attribute) != 0
#error "unknown C attributes must report unavailable"
#endif

struct [[nodiscard("check the error value")]] Error {
    int value;
};

enum [[nodiscard]] Status {
    STATUS_OK,
};

union [[nodiscard("inspect the response")]] Response {
    int code;
    unsigned int flags;
};

[[nodiscard("check the function result")]] int important(int value);
int important(int value) {
    return value;
}

int declared_earlier(void);

void call_before_attribute(void) {
    declared_earlier();
}

[[nodiscard("check the redeclared result")]] int declared_earlier(void);
int declared_earlier(void) {
    return 1;
}

struct Error make_error(void) {
    return (struct Error){ 1 };
}

enum Status make_status(void) {
    return STATUS_OK;
}

union Response make_response(void) {
    return (union Response){ .code = 0 };
}

int post_declarator [[nodiscard("post-declarator message")]] (void);
int post_declarator(void) {
    return 2;
}

void check_discarded_results(void) {
    important(1);
    (void)important(2);
    int used = important(3);
    make_error();
    make_status();
    make_response();
    declared_earlier();
    post_declarator();
    (void)used;
}

int main(void) {
    struct Error error = make_error();
    check_discarded_results();
    return error.value == 1 ? 0 : 1;
}
