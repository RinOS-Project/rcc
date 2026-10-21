#include <pthread.h>
#include <stdint.h>

extern int verified_thread_local_static_worker(int initial_value);

static void* run_worker(void* context)
{
    int initial_value = *(const int*)context;
    int result = verified_thread_local_static_worker(initial_value);
    return (void*)(intptr_t)result;
}

static int run_fresh_thread(int initial_value)
{
    pthread_t thread;
    void* result = NULL;
    if (pthread_create(&thread, NULL, run_worker, &initial_value) != 0) {
        return 1;
    }
    if (pthread_join(thread, &result) != 0) return 2;
    return (int)(intptr_t)result;
}

int main(void)
{
    if (run_fresh_thread(13) != 0) return 1;
    if (run_fresh_thread(79) != 0) return 2;
    return 0;
}
