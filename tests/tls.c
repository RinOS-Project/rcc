_Thread_local int tls_counter = 7;
static _Thread_local int tls_zero;

int main(void) {
    tls_counter += 5;
    tls_zero = 3;
    return tls_counter - (int)tls_zero;
}
