#define _GNU_SOURCE 1
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern void movdqu_copy(const char* source, char* destination);

static int bytes_match(const char* actual, const char* expected) {
    for (int i = 0; i < 16; ++i) {
        if (actual[i] != expected[i]) return 0;
    }
    return 1;
}

int main(void) {
    long page_result = sysconf(_SC_PAGESIZE);
    size_t page_size;
    char* mapping;
    char destination[16];
    char* page_boundary;
    pid_t child;
    int child_status;
    struct rlimit core_limit = {0, 0};

    if (page_result <= 0) return 1;
    page_size = (size_t)page_result;
    if (page_size < 64u || page_size > SIZE_MAX / 2u) return 2;

    mapping = (char*)mmap(NULL, page_size * 2u, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return 3;
    page_boundary = mapping + page_size;
    if (mprotect(page_boundary, page_size, PROT_NONE) != 0) {
        munmap(mapping, page_size * 2u);
        return 4;
    }

    for (int i = 0; i < 32; ++i) {
        mapping[i] = (char)(i * 11 + 5);
        page_boundary[-32 + i] = (char)(i * 13 + 9);
    }

    movdqu_copy(mapping, destination);
    if (!bytes_match(destination, mapping)) {
        munmap(mapping, page_size * 2u);
        return 5;
    }
    movdqu_copy(mapping + 1, destination);
    if (!bytes_match(destination, mapping + 1)) {
        munmap(mapping, page_size * 2u);
        return 6;
    }

    /* A 16-byte load ending exactly at the readable page boundary is valid. */
    movdqu_copy(page_boundary - 16, destination);
    if (!bytes_match(destination, page_boundary - 16)) {
        munmap(mapping, page_size * 2u);
        return 7;
    }

    (void)setrlimit(RLIMIT_CORE, &core_limit);
    child = fork();
    if (child < 0) {
        munmap(mapping, page_size * 2u);
        return 8;
    }
    if (child == 0) {
        /* The final byte crosses into PROT_NONE and must raise a page fault. */
        movdqu_copy(page_boundary - 15, destination);
        _exit(0);
    }
    if (waitpid(child, &child_status, 0) != child) {
        munmap(mapping, page_size * 2u);
        return 9;
    }
    if (!WIFSIGNALED(child_status) ||
        (WTERMSIG(child_status) != SIGSEGV &&
         WTERMSIG(child_status) != SIGBUS)) {
        munmap(mapping, page_size * 2u);
        return 10;
    }

    if (munmap(mapping, page_size * 2u) != 0) return 11;
    return 0;
}
