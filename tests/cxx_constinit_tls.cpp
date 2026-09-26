thread_local constinit int tls_global = 11;

int read_constinit_tls(void) {
    return tls_global;
}
