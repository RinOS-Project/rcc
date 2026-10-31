#include <pthread.h>

extern void verified_atomic_pair_reset(void);
extern void verified_atomic_pair_increment(unsigned int iterations);
extern unsigned long long verified_atomic_pair_read(void);
extern int verified_atomic_double_store_load(void);
extern int verified_atomic_bool_store_load(void);
extern int verified_atomic_ref_char(void);
extern int verified_constructor_parameter_binding(void);
extern int verified_atomic_lock_free_query(unsigned long size,
                                           const void* object);
extern int verified_atomic_always_lock_free(void);

enum { THREAD_COUNT = 4, ITERATIONS_PER_THREAD = 25000 };

static void* run_incrementer(void* unused)
{
    (void)unused;
    verified_atomic_pair_increment(ITERATIONS_PER_THREAD);
    return NULL;
}

int main(void)
{
    pthread_t threads[THREAD_COUNT];
    unsigned int created = 0u;
    unsigned char lock_free_object = 0u;

    if (verified_atomic_double_store_load() != 0) return 5;
    if (verified_atomic_bool_store_load() != 0) return 9;
    if (verified_atomic_ref_char() != 0) return 6;
    if (verified_constructor_parameter_binding() != 0) return 8;
    if (!verified_atomic_always_lock_free() ||
        !verified_atomic_lock_free_query(1u, &lock_free_object) ||
        !verified_atomic_lock_free_query(2u, &lock_free_object) ||
        !verified_atomic_lock_free_query(4u, &lock_free_object) ||
        !verified_atomic_lock_free_query(8u, &lock_free_object) ||
        verified_atomic_lock_free_query(3u, &lock_free_object) ||
        verified_atomic_lock_free_query(16u, &lock_free_object)) return 7;
    verified_atomic_pair_reset();
    if (verified_atomic_pair_read() != 0ULL) return 1;

    for (; created < THREAD_COUNT; ++created) {
        if (pthread_create(&threads[created], NULL,
                           run_incrementer, NULL) != 0) {
            while (created > 0u) {
                --created;
                (void)pthread_join(threads[created], NULL);
            }
            return 2;
        }
    }
    for (unsigned int index = 0u; index < THREAD_COUNT; ++index) {
        if (pthread_join(threads[index], NULL) != 0) return 3;
    }

    return verified_atomic_pair_read() ==
        (((unsigned long long)THREAD_COUNT * ITERATIONS_PER_THREAD) << 32 |
         ((unsigned long long)THREAD_COUNT * ITERATIONS_PER_THREAD * 7u))
        ? 0 : 4;
}
