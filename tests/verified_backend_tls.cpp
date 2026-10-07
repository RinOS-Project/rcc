extern "C" {
thread_local int verified_fallback_tls = 7;

int verified_fallback_read(void)
{
    return verified_fallback_tls;
}

int verified_fallback_increment(void)
{
    return ++verified_fallback_tls;
}

void verified_fallback_write(int value)
{
    verified_fallback_tls = value;
}
}
