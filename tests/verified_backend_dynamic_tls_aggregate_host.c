#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <stdint.h>

extern int verified_dynamic_tls_aggregate_probe(int expected_value,
                                                 int next_value);
extern volatile int* verified_dynamic_tls_aggregate_address(void);
extern volatile int* verified_global_tls_aggregate_address(void);

int verified_dynamic_tls_aggregate_destructor_count;

typedef struct SharedState {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int ready;
    int release;
    volatile int* local_addresses[2];
    volatile int* global_addresses[2];
} SharedState;

typedef struct WorkerContext {
    int value;
    int slot;
    SharedState* shared;
} WorkerContext;

static void* run_dynamic_tls_aggregate(void* argument) {
    WorkerContext* context = (WorkerContext*)argument;
    SharedState* shared = context->shared;
    volatile int* local_address = NULL;
    volatile int* global_address = NULL;
    int result = 0;
    int probe_result = verified_dynamic_tls_aggregate_probe(0,
                                                            context->value);

    if (probe_result != 0) {
        result = 10 + probe_result;
    } else {
        local_address = verified_dynamic_tls_aggregate_address();
        global_address = verified_global_tls_aggregate_address();
        if (!local_address || *local_address != context->value ||
            !global_address || *global_address != context->value + 1000) {
            result = 2;
        } else {
            probe_result = verified_dynamic_tls_aggregate_probe(
                context->value, context->value + 100);
            if (probe_result != 0) {
                result = 20 + probe_result;
            } else if (verified_dynamic_tls_aggregate_address() !=
                           local_address ||
                       *local_address != context->value + 100 ||
                       verified_global_tls_aggregate_address() !=
                           global_address ||
                       *global_address != context->value + 1100) {
                result = 4;
            }
        }
    }

    pthread_mutex_lock(&shared->mutex);
    shared->local_addresses[context->slot] = local_address;
    shared->global_addresses[context->slot] = global_address;
    ++shared->ready;
    pthread_cond_broadcast(&shared->condition);
    while (!shared->release) {
        pthread_cond_wait(&shared->condition, &shared->mutex);
    }
    pthread_mutex_unlock(&shared->mutex);

    if (((shared->local_addresses[0] && shared->local_addresses[1] &&
          shared->local_addresses[0] == shared->local_addresses[1]) ||
         (shared->global_addresses[0] && shared->global_addresses[1] &&
          shared->global_addresses[0] == shared->global_addresses[1])) &&
        result == 0) {
        result = 5;
    }
    return (void*)(intptr_t)result;
}

int main(void) {
    SharedState shared = {PTHREAD_MUTEX_INITIALIZER,
                          PTHREAD_COND_INITIALIZER, 0, 0,
                          {NULL, NULL}, {NULL, NULL}};
    WorkerContext contexts[2] = {{13, 0, &shared}, {79, 1, &shared}};
    pthread_t threads[2];
    void* results[2] = {NULL, NULL};
    int created = 0;

    for (; created < 2; ++created) {
        if (pthread_create(&threads[created], NULL,
                           run_dynamic_tls_aggregate,
                           &contexts[created]) != 0) {
            break;
        }
    }

    pthread_mutex_lock(&shared.mutex);
    while (shared.ready < created) {
        pthread_cond_wait(&shared.condition, &shared.mutex);
    }
    shared.release = 1;
    pthread_cond_broadcast(&shared.condition);
    pthread_mutex_unlock(&shared.mutex);

    for (int index = 0; index < created; ++index) {
        if (pthread_join(threads[index], &results[index]) != 0) return 2;
        if (results[index]) return 10 + (int)(intptr_t)results[index];
    }
    if (created != 2) return 3;
    if (!shared.local_addresses[0] || !shared.local_addresses[1] ||
        shared.local_addresses[0] == shared.local_addresses[1] ||
        !shared.global_addresses[0] || !shared.global_addresses[1] ||
        shared.global_addresses[0] == shared.global_addresses[1]) return 4;
    if (verified_dynamic_tls_aggregate_destructor_count != 4) return 5;

    pthread_cond_destroy(&shared.condition);
    pthread_mutex_destroy(&shared.mutex);
    return 0;
}
