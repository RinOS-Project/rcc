extern _Thread_local int verified_fallback_tls;

int verified_tls_import_read(void)
{
    return verified_fallback_tls;
}
