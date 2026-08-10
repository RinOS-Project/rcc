/* Simple RinOS test program for RCC */

/* Syscall numbers */
#define SYS_EXIT   0
#define SYS_WRITE  23

/* Syscall interface */
static int syscall1(int num, int arg1) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
    );
    return ret;
}

static int syscall3(int num, int arg1, int arg2, int arg3) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
    );
    return ret;
}

void _start(void) {
    /* Write "Hello from RCC!\n" to stdout */
    const char msg[] = "Hello from RCC!\n";
    syscall3(SYS_WRITE, 1, (int)msg, sizeof(msg) - 1);
    
    /* Exit with code 0 */
    syscall1(SYS_EXIT, 0);
    
    /* Should not reach here */
    for(;;) {}
}
