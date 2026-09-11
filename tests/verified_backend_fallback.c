_Thread_local int verified_fallback_tls = 7;

int verified_fallback_read(void)
{
    return verified_fallback_tls;
}
