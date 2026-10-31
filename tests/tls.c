_Thread_local int tls_counter = 7;
__thread int gnu_tls_counter = 9;
static _Thread_local int tls_zero;
int normal_data = 3;

static void* __attribute__((used)) gnu_attribute_after_type(void* value) {
    return value;
}
static void __attribute__((naked, used)) gnu_naked_after_type(void) {
    __asm__ volatile("ud2");
}

int main(void) {
    tls_counter += 5;
    gnu_tls_counter += 3;
    tls_zero = 3;
    return tls_counter + gnu_tls_counter - (int)tls_zero;
}
