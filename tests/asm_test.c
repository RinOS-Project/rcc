/* Test inline assembly support */

int get_cpu_info(void) {
    int result;
    __asm__ __volatile__("cpuid" : "=a"(result) : "a"(0));
    return result;
}

void halt_cpu(void) {
    __asm__ __volatile__("cli; hlt");
}

int do_syscall(int num, int arg1) {
    int result;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(num), "b"(arg1)
    );
    return result;
}

int main(void) {
    int info = get_cpu_info();
    int res = do_syscall(0, 42);
    return info + res;
}
