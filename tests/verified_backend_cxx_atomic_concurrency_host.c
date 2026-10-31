#include <pthread.h>

extern void verified_atomic_pair_reset(void);
extern void verified_atomic_pair_increment(unsigned int iterations);
extern unsigned long long verified_atomic_pair_read(void);

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
