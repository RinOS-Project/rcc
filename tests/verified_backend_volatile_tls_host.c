#include <pthread.h>

extern int verified_volatile_tls_aggregate(int value);
extern int verified_cpp_volatile_tls_aggregate(int value);

typedef struct {
    int input;
    int result;
} ThreadContext;

static void* run_tls_worker(void* opaque)
{
    ThreadContext* context = (ThreadContext*)opaque;
    context->result = verified_volatile_tls_aggregate(context->input) +
        verified_cpp_volatile_tls_aggregate(context->input);
    return NULL;
}

int main(void)
{
    pthread_t first_thread;
    pthread_t second_thread;
    ThreadContext first = {31, 0};
    ThreadContext second = {47, 0};

    if (verified_volatile_tls_aggregate(11) != 35 ||
        verified_volatile_tls_aggregate(13) != 38 ||
        verified_cpp_volatile_tls_aggregate(11) != 41 ||
        verified_cpp_volatile_tls_aggregate(13) != 44) {
        return 1;
    }
    if (pthread_create(&first_thread, NULL, run_tls_worker, &first) != 0)
        return 2;
    if (pthread_create(&second_thread, NULL, run_tls_worker, &second) != 0) {
        (void)pthread_join(first_thread, NULL);
        return 3;
    }
    if (pthread_join(first_thread, NULL) != 0 ||
        pthread_join(second_thread, NULL) != 0) {
        return 4;
    }
    return first.result == 116 && second.result == 148 ? 0 : 5;
}
