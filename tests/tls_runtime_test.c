#include <pthread.h>

int tls_runtime_global_value(void);
int tls_runtime_private_value(void);
int tls_runtime_update(int delta);
int tls_runtime_block_sequence(void);

typedef struct TlsThreadCase {
    int delta;
    int status;
} TlsThreadCase;

static void* tls_runtime_worker(void* opaque)
{
    TlsThreadCase* test_case = (TlsThreadCase*)opaque;
    if (tls_runtime_global_value() != 17 ||
        tls_runtime_private_value() != 29 ||
        tls_runtime_block_sequence() != 41 ||
        tls_runtime_update(test_case->delta) != 17 + test_case->delta ||
        tls_runtime_private_value() != 29 + test_case->delta ||
        tls_runtime_block_sequence() != 42) {
        test_case->status = 1;
    }
    return NULL;
}

int main(void)
{
    pthread_t first_thread;
    pthread_t second_thread;
    TlsThreadCase first_case = {100, 0};
    TlsThreadCase second_case = {-5, 0};
    if (pthread_create(&first_thread, NULL, tls_runtime_worker,
                       &first_case) != 0) {
        return 1;
    }
    if (pthread_create(&second_thread, NULL, tls_runtime_worker,
                       &second_case) != 0) {
        pthread_join(first_thread, NULL);
        return 1;
    }
    if (pthread_join(first_thread, NULL) != 0 ||
        pthread_join(second_thread, NULL) != 0) {
        return 1;
    }
    if (first_case.status || second_case.status ||
        tls_runtime_global_value() != 17 ||
        tls_runtime_private_value() != 29 ||
        tls_runtime_block_sequence() != 41 ||
        tls_runtime_block_sequence() != 42) {
        return 2;
    }
    return 0;
}
