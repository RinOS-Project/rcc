/* RIN v3 dynamic-TLS relocation format integration fixture. */
_Thread_local int rin_dynamic_tls_counter = 7;

int rin_dynamic_tls_read(void)
{
    return rin_dynamic_tls_counter;
}

void rin_dynamic_tls_write(int value)
{
    rin_dynamic_tls_counter = value;
}
