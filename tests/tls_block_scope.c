_Thread_local int external_counter;

int tls_block_scope(void)
{
    static _Thread_local int counter = 7;
    extern _Thread_local int external_counter;
    counter += 2;
    return counter + (external_counter == 0);
}

int main(void)
{
    return tls_block_scope();
}
