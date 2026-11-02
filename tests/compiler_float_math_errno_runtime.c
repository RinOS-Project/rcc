/* Host-side single-threaded errno storage for running RinOS-targeted math
 * assembly.  The target libc normally supplies this TLS-backed function;
 * this runner only needs the same writable errno object for the test. */
static int compiler_float_math_errno;

int* __errno_location(void) {
    return &compiler_float_math_errno;
}
