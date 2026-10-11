_Thread_local int verified_fallback_tls = 7;

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

int verified_fallback_local_tls_increment(void)
{
    static _Thread_local int value = 19;
    return ++value;
}

int verified_fallback_local_static_increment(void)
{
    static int value = 23;
    return ++value;
}
