int verified_atomic_lock_free_query(unsigned long size,
                                    const void* object)
{
    return __atomic_is_lock_free(size, object);
}

int verified_atomic_always_lock_free(void)
{
    unsigned int object = 0u;
    return __atomic_always_lock_free(sizeof(object), &object);
}
