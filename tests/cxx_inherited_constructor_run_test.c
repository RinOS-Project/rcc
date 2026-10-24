#include <assert.h>
#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_inherited_constructor_probe(void);
extern int cxx_inherited_template_order_state;
extern int cxx_inherited_template_order_error;
extern int cxx_inherited_template_order_complete;

int main(void) {
    int result = cxx_inherited_constructor_probe();
    if (result != 165 || cxx_inherited_template_order_state != 0 ||
        cxx_inherited_template_order_error != 0 ||
        cxx_inherited_template_order_complete != 4) {
        fprintf(stderr,
                "inherited constructor probe returned %d; order state=%d, error=%d, completed=%d\n",
                result, cxx_inherited_template_order_state,
                cxx_inherited_template_order_error,
                cxx_inherited_template_order_complete);
        return 1;
    }
    puts("C++ inherited constructor execution test passed");
    return 0;
}
