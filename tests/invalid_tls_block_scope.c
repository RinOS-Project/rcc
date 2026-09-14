int invalid_tls_block_scope(void)
{
    _Thread_local int value;
    return value;
}
