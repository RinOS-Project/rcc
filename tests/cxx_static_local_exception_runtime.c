#include <elf.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN64)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

typedef void (RIN_SYSV *CxxStaticLocalCallback)(void*);

typedef struct CxxStaticLocalExit {
    CxxStaticLocalCallback callback;
    void* argument;
    void* dso;
    int active;
} CxxStaticLocalExit;

static CxxStaticLocalExit cxx_static_local_exits[8];
static size_t cxx_static_local_exit_count;
static CxxStaticLocalExit cxx_thread_local_exits[8];
static size_t cxx_thread_local_exit_count;
void* __dso_handle;

int cxx_static_local_attempts;
int cxx_static_local_guard_aborts;
int cxx_static_local_guard_releases;
int cxx_static_local_registrations;
int cxx_static_local_finalize_calls;
int cxx_static_local_dso_mismatches;
int cxx_static_local_registration_dso_mismatches;
int cxx_static_local_destructors;
int cxx_static_local_destruction_order;
int cxx_thread_local_attempts;
int cxx_thread_local_destructors;
int cxx_thread_local_registrations;
int cxx_thread_local_dso_mismatches;
int cxx_thread_local_destruction_order;

static uintptr_t rin_test_tls_anchor;
static unsigned char rin_test_tls_storage[65536]
    __attribute__((aligned(4096)));

/* The exception tests use a tiny custom _start instead of libc.  Give the
 * generated local-exec TLS code a real per-process TLS base so the same
 * function-local thread_local retry path can run on the Linux host. */
int rin_test_setup_tls(uintptr_t* initial_stack)
{
    uintptr_t* cursor = initial_stack;
    uintptr_t argc;
    uintptr_t phdr_address = 0u;
    uintptr_t phnum = 0u;
    uintptr_t phent = 0u;
    uintptr_t tls_pointer = 0u;
    if (!cursor) return -1;
    argc = *cursor++;
    if (argc > 4096u) return -1;
    cursor += argc;
    if (*cursor++ != 0u) return -1;
    {
        size_t count = 0u;
        while (*cursor != 0u && count++ < 4096u) ++cursor;
        if (*cursor++ != 0u) return -1;
    }
    for (size_t count = 0u; count < 256u; ++count) {
        uintptr_t kind = cursor[0];
        uintptr_t value = cursor[1];
        cursor += 2;
        if (kind == AT_NULL) break;
        if (kind == AT_PHDR) phdr_address = value;
        else if (kind == AT_PHNUM) phnum = value;
        else if (kind == AT_PHENT) phent = value;
    }
    if (!phdr_address || !phnum || phnum > 256u || phent == 0u) return -1;
#if UINTPTR_MAX == UINT64_MAX
    if (phent != sizeof(Elf64_Phdr)) return -1;
    for (uintptr_t index = 0u; index < phnum; ++index) {
        const Elf64_Phdr* header = (const Elf64_Phdr*)(
            phdr_address + index * phent);
        if (header->p_type != PT_TLS) continue;
        uintptr_t storage_start = (uintptr_t)rin_test_tls_storage;
        uintptr_t storage_end = storage_start + sizeof(rin_test_tls_storage);
        if (header->p_memsz == 0u || header->p_filesz > header->p_memsz ||
            header->p_memsz > sizeof(rin_test_tls_storage) ||
            header->p_align == 0u || header->p_align > 4096u ||
            (header->p_align & (header->p_align - 1u)) != 0u ||
            header->p_vaddr > UINTPTR_MAX - header->p_filesz ||
            storage_start > UINTPTR_MAX - sizeof(rin_test_tls_storage)) return -1;
        {
            uintptr_t block_size = (uintptr_t)header->p_memsz;
            if (block_size > UINTPTR_MAX - ((uintptr_t)header->p_align - 1u))
                return -1;
            block_size = (block_size + (uintptr_t)header->p_align - 1u) &
                ~((uintptr_t)header->p_align - 1u);
            if (block_size > sizeof(rin_test_tls_storage)) return -1;
            tls_pointer = storage_start + block_size;
        }
        if (tls_pointer > storage_end) return -1;
        {
            unsigned char* destination = rin_test_tls_storage;
            const unsigned char* source = (const unsigned char*)(
                uintptr_t)header->p_vaddr;
            for (uintptr_t byte = 0u; byte < header->p_filesz; ++byte)
                destination[byte] = source[byte];
            for (uintptr_t byte = (uintptr_t)header->p_filesz;
                 byte < (uintptr_t)header->p_memsz; ++byte)
                destination[byte] = 0u;
        }
        break;
    }
    if (!tls_pointer) return -1;
    rin_test_tls_anchor = tls_pointer;
    {
        long result;
        __asm__ volatile("syscall"
                         : "=a"(result)
                         : "0"(158L), "D"(0x1002L),
                           "S"(&rin_test_tls_anchor)
                         : "rcx", "r11", "memory");
        return result < 0 ? (int)result : 0;
    }
#else
    if (phent != sizeof(Elf32_Phdr)) return -1;
    for (uintptr_t index = 0u; index < phnum; ++index) {
        const Elf32_Phdr* header = (const Elf32_Phdr*)(
            phdr_address + index * phent);
        if (header->p_type != PT_TLS) continue;
        uintptr_t storage_start = (uintptr_t)rin_test_tls_storage;
        uintptr_t storage_end = storage_start + sizeof(rin_test_tls_storage);
        if (header->p_memsz == 0u || header->p_filesz > header->p_memsz ||
            header->p_memsz > sizeof(rin_test_tls_storage) ||
            header->p_align == 0u || header->p_align > 4096u ||
            (header->p_align & (header->p_align - 1u)) != 0u ||
            header->p_vaddr > UINTPTR_MAX - header->p_filesz ||
            storage_start > UINTPTR_MAX - sizeof(rin_test_tls_storage)) return -1;
        {
            uintptr_t block_size = (uintptr_t)header->p_memsz;
            if (block_size > UINTPTR_MAX - ((uintptr_t)header->p_align - 1u))
                return -1;
            block_size = (block_size + (uintptr_t)header->p_align - 1u) &
                ~((uintptr_t)header->p_align - 1u);
            if (block_size > sizeof(rin_test_tls_storage)) return -1;
            tls_pointer = storage_start + block_size;
        }
        if (tls_pointer > storage_end) return -1;
        {
            unsigned char* destination = rin_test_tls_storage;
            const unsigned char* source = (const unsigned char*)(
                uintptr_t)header->p_vaddr;
            for (uintptr_t byte = 0u; byte < header->p_filesz; ++byte)
                destination[byte] = source[byte];
            for (uintptr_t byte = (uintptr_t)header->p_filesz;
                 byte < (uintptr_t)header->p_memsz; ++byte)
                destination[byte] = 0u;
        }
        break;
    }
    if (!tls_pointer) return -1;
    rin_test_tls_anchor = tls_pointer;
    {
        uint32_t descriptor[4] = {
            UINT32_MAX, (uint32_t)(uintptr_t)&rin_test_tls_anchor,
            0x000fffffu, 0x00000051u
        };
        long result;
        uint16_t selector;
        __asm__ volatile("int $0x80"
                         : "=a"(result)
                         : "0"(243L), "b"(descriptor)
                         : "memory", "cc");
        if (result < 0) return (int)result;
        selector = (uint16_t)((descriptor[0] << 3) | 3u);
        __asm__ volatile("movw %0, %%gs" : : "rm"(selector) : "memory");
        return 0;
    }
#endif
}

int RIN_SYSV __cxa_guard_acquire(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    if (state[0] != 0u) return 0;
    if (state[1] != 0u) return 0;
    state[1] = 1u;
    return 1;
}

void RIN_SYSV __cxa_guard_release(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    state[0] = 1u;
    state[1] = 0u;
    ++cxx_static_local_guard_releases;
}

void RIN_SYSV __cxa_guard_abort(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    state[1] = 0u;
    ++cxx_static_local_guard_aborts;
}

int RIN_SYSV __cxa_atexit(CxxStaticLocalCallback callback,
                          void* argument, void* dso)
{
    CxxStaticLocalExit* entry;
    if (!callback || !argument ||
        cxx_static_local_exit_count >=
            sizeof(cxx_static_local_exits) / sizeof(cxx_static_local_exits[0])) {
        return -1;
    }
    if (dso != &__dso_handle)
        ++cxx_static_local_registration_dso_mismatches;
    entry = &cxx_static_local_exits[cxx_static_local_exit_count++];
    entry->callback = callback;
    entry->argument = argument;
    entry->dso = dso;
    entry->active = 1;
    ++cxx_static_local_registrations;
    return 0;
}

void RIN_SYSV __cxa_finalize(void* dso)
{
    size_t index = cxx_static_local_exit_count;
    ++cxx_static_local_finalize_calls;
    while (index > 0u) {
        CxxStaticLocalExit* entry = &cxx_static_local_exits[--index];
        if (!entry->active) continue;
        if (dso && entry->dso != dso) {
            ++cxx_static_local_dso_mismatches;
            continue;
        }
        entry->active = 0;
        entry->callback(entry->argument);
    }
}

int RIN_SYSV __cxa_thread_atexit(CxxStaticLocalCallback callback,
                                 void* argument, void* dso)
{
    CxxStaticLocalExit* entry;
    if (!callback || !argument ||
        cxx_thread_local_exit_count >=
            sizeof(cxx_thread_local_exits) /
                sizeof(cxx_thread_local_exits[0])) {
        return -1;
    }
    if (dso != &__dso_handle) ++cxx_thread_local_dso_mismatches;
    entry = &cxx_thread_local_exits[cxx_thread_local_exit_count++];
    entry->callback = callback;
    entry->argument = argument;
    entry->dso = dso;
    entry->active = 1;
    ++cxx_thread_local_registrations;
    return 0;
}

void RIN_SYSV cxx_static_local_finalize(void)
{
    __cxa_finalize(&__dso_handle);
}

void RIN_SYSV cxx_static_local_thread_finalize(void)
{
    size_t index = cxx_thread_local_exit_count;
    while (index > 0u) {
        CxxStaticLocalExit* entry = &cxx_thread_local_exits[--index];
        if (!entry->active) continue;
        if (entry->dso != &__dso_handle) {
            ++cxx_thread_local_dso_mismatches;
            continue;
        }
        entry->active = 0;
        entry->callback(entry->argument);
    }
}
