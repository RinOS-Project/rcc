_Thread_local int tls_runtime_global = 17;
static _Thread_local int tls_runtime_private = 29;

int tls_runtime_global_value(void)
{
    return tls_runtime_global;
}

int tls_runtime_private_value(void)
{
    return tls_runtime_private;
}

int tls_runtime_update(int delta)
{
    tls_runtime_global += delta;
    tls_runtime_private += delta;
    return tls_runtime_global;
}

int tls_runtime_block_sequence(void)
{
    static _Thread_local int sequence = 40;
    return ++sequence;
}
