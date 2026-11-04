#define _GNU_SOURCE 1
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern void rcc_loadu_si16(const void*, void*);
extern void rcc_loadu_si32(const void*, void*);
extern void rcc_loadu_si64(const void*, void*);
extern void rcc_loadl_epi64(const void*, void*);
extern void rcc_storeu_si16(void*, const void*);
extern void rcc_storeu_si32(void*, const void*);
extern void rcc_storeu_si64(void*, const void*);
extern void rcc_storel_epi64(void*, const void*);

typedef void (*GuardOperation)(void* address);

static unsigned char guard_output[16];
static unsigned char guard_source[16];

static void cross_load_16(void* address) { rcc_loadu_si16(address, guard_output); }
static void cross_load_32(void* address) { rcc_loadu_si32(address, guard_output); }
static void cross_load_64(void* address) { rcc_loadu_si64(address, guard_output); }
static void cross_loadl_64(void* address) { rcc_loadl_epi64(address, guard_output); }
static void cross_store_16(void* address) { rcc_storeu_si16(address, guard_source); }
static void cross_store_32(void* address) { rcc_storeu_si32(address, guard_source); }
static void cross_store_64(void* address) { rcc_storeu_si64(address, guard_source); }
static void cross_storel_64(void* address) { rcc_storel_epi64(address, guard_source); }

static int crosses_guard_page(GuardOperation operation, void* address) {
    struct rlimit core_limit = {0, 0};
    pid_t child;
    int child_status;
    (void)setrlimit(RLIMIT_CORE, &core_limit);
    child = fork();
    if (child < 0) return 0;
    if (child == 0) {
        operation(address);
        _exit(0);
    }
    if (waitpid(child, &child_status, 0) != child) return 0;
    return WIFSIGNALED(child_status) &&
        (WTERMSIG(child_status) == SIGSEGV ||
         WTERMSIG(child_status) == SIGBUS);
}

static int expected_load(const unsigned char* output,
                         const unsigned char* input, size_t width) {
    for (size_t index = 0; index < sizeof(guard_output); ++index) {
        unsigned char expected = index < width ? input[index] : 0;
        if (output[index] != expected) return 0;
    }
    return 1;
}

static int expected_store(const unsigned char* output,
                          const unsigned char* input, size_t width) {
    for (size_t index = 0; index < width; ++index) {
        if (output[index] != input[index]) return 0;
    }
    return 1;
}

int main(void) {
    long page_result = sysconf(_SC_PAGESIZE);
    size_t page_size;
    unsigned char* mapping;
    unsigned char* boundary;
    unsigned char* source;
    GuardOperation loads[4] = {
        cross_load_16, cross_load_32, cross_load_64, cross_loadl_64
    };
    GuardOperation stores[4] = {
        cross_store_16, cross_store_32, cross_store_64, cross_storel_64
    };
    size_t widths[4] = {2, 4, 8, 8};

    if (page_result <= 0) return 1;
    page_size = (size_t)page_result;
    if (page_size < 64u || page_size > SIZE_MAX / 2u) return 2;
    mapping = (unsigned char*)mmap(NULL, page_size * 2u,
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return 3;
    boundary = mapping + page_size;
    if (mprotect(boundary, page_size, PROT_NONE) != 0) {
        munmap(mapping, page_size * 2u);
        return 4;
    }
    source = mapping + 1;
    for (size_t index = 0; index < sizeof(guard_source); ++index) {
        source[index] = (unsigned char)(0x31u + index * 9u);
        guard_source[index] = (unsigned char)(0x82u + index * 7u);
    }

    for (size_t operation = 0; operation < 4; ++operation) {
        size_t width = widths[operation];
        unsigned char* exact = boundary - width;
        for (size_t index = 0; index < width; ++index) {
            exact[index] = (unsigned char)(0x43u + index * 5u);
        }
        for (size_t index = 0; index < sizeof(guard_output); ++index) {
            guard_output[index] = 0xCC;
        }
        loads[operation](exact);
        if (!expected_load(guard_output, exact, width)) {
            munmap(mapping, page_size * 2u);
            return 5;
        }

        for (size_t index = 0; index < width; ++index) exact[index] = 0xA5;
        stores[operation](exact);
        if (!expected_store(exact, guard_source, width)) {
            munmap(mapping, page_size * 2u);
            return 6;
        }

        if (!crosses_guard_page(loads[operation], boundary - width + 1u)) {
            munmap(mapping, page_size * 2u);
            return 7;
        }
        if (!crosses_guard_page(stores[operation], boundary - width + 1u)) {
            munmap(mapping, page_size * 2u);
            return 8;
        }
    }

    if (munmap(mapping, page_size * 2u) != 0) return 9;
    return 0;
}
