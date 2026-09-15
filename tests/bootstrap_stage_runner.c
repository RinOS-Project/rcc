/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE

#include "rin_formats_v3.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*RccEntry)(int argc, char** argv);

/* The stage image imports the same target-width exception ABI as rincrt.  The
 * runner owns a hosted implementation so a bootstrap image can exercise the
 * ABI without linking against a second copy of the SDK runtime. */
#if defined(__x86_64__)
typedef long RccBootstrapJmpBuf[8];
#else
typedef long RccBootstrapJmpBuf[6];
#endif

typedef void (*RccBootstrapCleanup)(void* object);
typedef struct RccBootstrapCleanupRecord {
    RccBootstrapCleanup callback;
    void* object;
    struct RccBootstrapCleanupRecord* previous;
} RccBootstrapCleanupRecord;

typedef struct RccBootstrapExceptionFrame {
    RccBootstrapJmpBuf env;
    struct RccBootstrapExceptionFrame* previous;
    uintptr_t value;
    uintptr_t type;
    RccBootstrapCleanupRecord* cleanup_top;
} RccBootstrapExceptionFrame;

#define RCC_BOOTSTRAP_OBJECT_FLAG ((uintptr_t)UINT32_C(0x80000000))

static __thread RccBootstrapExceptionFrame* bootstrap_exception_top;
static __thread uintptr_t bootstrap_exception_value;
static __thread uintptr_t bootstrap_exception_type;

#if defined(__x86_64__)
__attribute__((naked, returns_twice))
int rcc_bootstrap_setjmp(RccBootstrapJmpBuf env __attribute__((unused)))
{
    __asm__ __volatile__(
        "mov %rdi, %rax\n"
        "mov %rbx, 0(%rax)\n"
        "mov %rbp, 8(%rax)\n"
        "mov %r12, 16(%rax)\n"
        "mov %r13, 24(%rax)\n"
        "mov %r14, 32(%rax)\n"
        "mov %r15, 40(%rax)\n"
        "lea 8(%rsp), %rdx\n"
        "mov %rdx, 48(%rax)\n"
        "mov (%rsp), %rdx\n"
        "mov %rdx, 56(%rax)\n"
        "xor %eax, %eax\n"
        "ret\n");
}

__attribute__((naked, noreturn))
void rcc_bootstrap_longjmp(RccBootstrapJmpBuf env __attribute__((unused)),
                           int value __attribute__((unused)))
{
    __asm__ __volatile__(
        "mov %rdi, %rdx\n"
        "mov %esi, %eax\n"
        "test %eax, %eax\n"
        "jnz 1f\n"
        "mov $1, %eax\n"
        "1:\n"
        "mov 0(%rdx), %rbx\n"
        "mov 8(%rdx), %rbp\n"
        "mov 16(%rdx), %r12\n"
        "mov 24(%rdx), %r13\n"
        "mov 32(%rdx), %r14\n"
        "mov 40(%rdx), %r15\n"
        "mov 48(%rdx), %rsp\n"
        "mov 56(%rdx), %rcx\n"
        "jmp *%rcx\n");
}
#else
__attribute__((naked, returns_twice))
int rcc_bootstrap_setjmp(RccBootstrapJmpBuf env __attribute__((unused)))
{
    __asm__ __volatile__(
        "mov 4(%esp), %eax\n"
        "mov %ebx, 0(%eax)\n"
        "mov %esi, 4(%eax)\n"
        "mov %edi, 8(%eax)\n"
        "mov %ebp, 12(%eax)\n"
        "lea 4(%esp), %edx\n"
        "mov %edx, 16(%eax)\n"
        "mov (%esp), %edx\n"
        "mov %edx, 20(%eax)\n"
        "xor %eax, %eax\n"
        "ret\n");
}

__attribute__((naked, noreturn))
void rcc_bootstrap_longjmp(RccBootstrapJmpBuf env __attribute__((unused)),
                           int value __attribute__((unused)))
{
    __asm__ __volatile__(
        "mov 4(%esp), %edx\n"
        "mov 8(%esp), %eax\n"
        "test %eax, %eax\n"
        "jnz 1f\n"
        "mov $1, %eax\n"
        "1:\n"
        "mov 0(%edx), %ebx\n"
        "mov 4(%edx), %esi\n"
        "mov 8(%edx), %edi\n"
        "mov 12(%edx), %ebp\n"
        "mov 16(%edx), %esp\n"
        "mov 20(%edx), %ecx\n"
        "jmp *%ecx\n");
}
#endif

static void bootstrap_exception_unwind(
    RccBootstrapExceptionFrame* frame)
{
    RccBootstrapCleanupRecord* record;
    if (!frame) exit(134);
    record = frame->cleanup_top;
    frame->cleanup_top = NULL;
    while (record) {
        RccBootstrapCleanupRecord* previous = record->previous;
        RccBootstrapCleanup callback = record->callback;
        void* object = record->object;
        free(record);
        if (!callback || !object) exit(134);
        callback(object);
        record = previous;
    }
}

void rin_cpp_exception_register_cleanup(
    RccBootstrapExceptionFrame* frame, RccBootstrapCleanup callback,
    void* object)
{
    RccBootstrapCleanupRecord* record;
    if (!frame || !callback || !object) exit(134);
    record = malloc(sizeof(*record));
    if (!record) exit(134);
    record->callback = callback;
    record->object = object;
    record->previous = frame->cleanup_top;
    frame->cleanup_top = record;
}

void rin_cpp_exception_unregister_cleanup(
    RccBootstrapExceptionFrame* frame, RccBootstrapCleanup callback,
    void* object)
{
    RccBootstrapCleanupRecord* record;
    RccBootstrapCleanupRecord* previous = NULL;
    if (!frame || !callback || !object) exit(134);
    for (record = frame->cleanup_top; record; record = record->previous) {
        if (record->callback == callback && record->object == object) {
            if (previous) previous->previous = record->previous;
            else frame->cleanup_top = record->previous;
            free(record);
            return;
        }
        previous = record;
    }
    exit(134);
}

void rin_cpp_exception_unwind_cleanups(RccBootstrapExceptionFrame* frame)
{
    bootstrap_exception_unwind(frame);
}

void rin_cpp_exception_install(RccBootstrapExceptionFrame* frame)
{
    if (!frame) exit(134);
    frame->previous = bootstrap_exception_top;
    frame->cleanup_top = NULL;
    bootstrap_exception_top = frame;
}

void rin_cpp_exception_leave(RccBootstrapExceptionFrame* frame)
{
    if (frame && bootstrap_exception_top == frame) {
        bootstrap_exception_top = frame->previous;
        bootstrap_exception_unwind(frame);
    }
}

__attribute__((noreturn))
void rin_cpp_exception_throw(uintptr_t value, uintptr_t type)
{
    RccBootstrapExceptionFrame* frame = bootstrap_exception_top;
    if (!frame) exit(1);
    bootstrap_exception_top = frame->previous;
    bootstrap_exception_unwind(frame);
    frame->value = value;
    frame->type = type;
    bootstrap_exception_value = value;
    bootstrap_exception_type = type;
    rcc_bootstrap_longjmp(frame->env, 1);
}

__attribute__((noreturn))
void rin_cpp_exception_throw_object(const void* object, uintptr_t size,
                                    uintptr_t type)
{
    unsigned char* copy;
    if (!object || size == 0u || (type & RCC_BOOTSTRAP_OBJECT_FLAG) == 0u) {
        exit(134);
    }
    copy = malloc((size_t)size);
    if (!copy) exit(134);
    memcpy(copy, object, (size_t)size);
    rin_cpp_exception_throw((uintptr_t)copy, type);
}

void rin_cpp_exception_release_frame(RccBootstrapExceptionFrame* frame)
{
    if (!frame || (frame->type & RCC_BOOTSTRAP_OBJECT_FLAG) == 0u ||
        frame->value == 0u) return;
    free((void*)frame->value);
    frame->value = 0u;
    frame->type = 0u;
}

__attribute__((noreturn))
void rin_cpp_exception_rethrow_frame(RccBootstrapExceptionFrame* frame)
{
    if (!frame || frame->type == 0u) exit(134);
    rin_cpp_exception_throw(frame->value, frame->type);
}

__attribute__((noreturn))
void rin_cpp_exception_rethrow(void)
{
    rin_cpp_exception_throw(bootstrap_exception_value,
                            bootstrap_exception_type);
}

static void fail(const char* message)
{
    fprintf(stderr, "bootstrap-stage-runner: %s\n", message);
    exit(2);
}

static int range_valid(uint64_t offset, uint64_t size, uint64_t capacity)
{
    return offset <= capacity && size <= capacity - offset;
}

static FILE* rin_stderr_adapter(void)
{
    return stderr;
}

static uintptr_t symbol_address(const char* name)
{
    void* address;
    if (strcmp(name, "setjmp") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rcc_bootstrap_setjmp;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "longjmp") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rcc_bootstrap_longjmp;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_install") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_install;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_leave") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_leave;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_throw") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_throw;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_rethrow") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_rethrow;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_throw_object") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_throw_object;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_rethrow_frame") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_rethrow_frame;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_release_frame") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_release_frame;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_register_cleanup") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_register_cleanup;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_unregister_cleanup") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_unregister_cleanup;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "rin_cpp_exception_unwind_cleanups") == 0) {
        uintptr_t result = 0u;
        void (*function)(void) = (void (*)(void))rin_cpp_exception_unwind_cleanups;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "__rin_stderr") == 0) {
        FILE* (*function)(void) = rin_stderr_adapter;
        uintptr_t result = 0u;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "atexit") == 0) {
        int (*function)(void (*)(void)) = atexit;
        uintptr_t result = 0u;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    dlerror();
    address = dlsym(RTLD_DEFAULT, name);
    if (!address || dlerror() != NULL) {
        fprintf(stderr, "bootstrap-stage-runner: unresolved import '%s'\n",
                name);
        exit(2);
    }
    return (uintptr_t)address;
}

static void protect_range(uint8_t* image, uint64_t image_size,
                          uint64_t offset, uint64_t size, int protection)
{
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t start;
    uintptr_t end;
    if (page_size <= 0 || !range_valid(offset, size, image_size)) {
        fail("invalid protection range");
    }
    start = ((uintptr_t)image + (uintptr_t)offset) &
            ~((uintptr_t)page_size - 1u);
    end = ((uintptr_t)image + (uintptr_t)(offset + size) +
           (uintptr_t)page_size - 1u) & ~((uintptr_t)page_size - 1u);
    if (end > start && mprotect((void*)start, end - start, protection) != 0) {
        fail("mprotect failed");
    }
}

static uint8_t* map_image(const uint8_t* file_bytes, size_t file_size,
                          const RinHeaderV3* header,
                          const RinSectionV3* sections)
{
    uint8_t* image;
    void* hint = header->architecture == RIN_ARCH_X86
        ? (void*)(uintptr_t)0x20000000u : NULL;
    image = mmap(hint, (size_t)header->image_size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (image == MAP_FAILED) fail("mmap failed");
    if (header->architecture == RIN_ARCH_X86 &&
        (uintptr_t)image + header->image_size >= UINT64_C(0xc0000000)) {
        fail("i686 image was not mapped below 3 GiB");
    }
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        if (section->type < RIN_IMAGE_SECTION_CODE ||
            section->type > RIN_IMAGE_SECTION_FINI_ARRAY ||
            section->file_size == 0u || section->memory_size == 0u) {
            continue;
        }
        if (!range_valid(section->file_offset, section->file_size, file_size) ||
            !range_valid(section->virtual_address, section->memory_size,
                         header->image_size) ||
            section->file_size > section->memory_size) {
            fail("invalid load section");
        }
        memcpy(image + section->virtual_address,
               file_bytes + section->file_offset,
               (size_t)section->file_size);
    }
    return image;
}

static void apply_relocations(uint8_t* image, const uint8_t* file_bytes,
                              size_t file_size, const RinHeaderV3* header,
                              const RinSectionV3* sections)
{
    uint64_t runtime_base = (uint64_t)(uintptr_t)image;
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        uint64_t count;
        const RinRelocationV3* relocations;
        if (section->type != RIN_IMAGE_SECTION_RELOCATIONS) continue;
        if (!range_valid(section->file_offset, section->file_size, file_size) ||
            section->file_size % sizeof(RinRelocationV3) != 0u) {
            fail("invalid relocation section");
        }
        relocations = (const RinRelocationV3*)(file_bytes +
                                               section->file_offset);
        count = section->file_size / sizeof(*relocations);
        for (uint64_t relocation_index = 0u;
             relocation_index < count; ++relocation_index) {
            const RinRelocationV3* relocation =
                &relocations[relocation_index];
            uint64_t preferred_target;
            uint64_t runtime_target;
            uint32_t width = relocation->type ==
                RIN_IMAGE_RELOCATION_ABS64 ? 8u : 4u;
            if (!range_valid(relocation->virtual_address, width,
                             header->image_size)) {
                fail("relocation target is outside image");
            }
            if (relocation->type == RIN_IMAGE_RELOCATION_ABS64) {
                memcpy(&preferred_target,
                       image + relocation->virtual_address, 8u);
            } else if (relocation->type == RIN_IMAGE_RELOCATION_ABS32U ||
                       relocation->type == RIN_IMAGE_RELOCATION_ABS32S) {
                uint32_t value;
                memcpy(&value, image + relocation->virtual_address, 4u);
                preferred_target = value;
            } else {
                fail("unsupported bootstrap relocation");
            }
            if (preferred_target < header->preferred_base ||
                preferred_target - header->preferred_base >=
                    header->image_size) {
                fail("absolute relocation points outside image");
            }
            runtime_target = runtime_base +
                (preferred_target - header->preferred_base);
            if (width == 8u) {
                memcpy(image + relocation->virtual_address,
                       &runtime_target, 8u);
            } else {
                uint32_t value;
                if (runtime_target > UINT32_MAX) {
                    fail("i686 relocation overflow");
                }
                value = (uint32_t)runtime_target;
                memcpy(image + relocation->virtual_address, &value, 4u);
            }
        }
    }
}

static void bind_imports(uint8_t* image, const uint8_t* file_bytes,
                         size_t file_size, const RinHeaderV3* header,
                         const RinSectionV3* sections)
{
    const char* strings;
    if (!range_valid(header->string_table_offset, header->string_table_size,
                     file_size)) {
        fail("invalid string table");
    }
    strings = (const char*)(file_bytes + header->string_table_offset);
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        const RinImportV3* imports;
        uint64_t count;
        uint32_t pointer_width = header->architecture == RIN_ARCH_X86
            ? 4u : 8u;
        if (section->type != RIN_IMAGE_SECTION_IMPORTS) continue;
        if (!range_valid(section->file_offset, section->file_size, file_size) ||
            section->file_size % sizeof(RinImportV3) != 0u) {
            fail("invalid import section");
        }
        imports = (const RinImportV3*)(file_bytes + section->file_offset);
        count = section->file_size / sizeof(*imports);
        for (uint64_t import_index = 0u; import_index < count; ++import_index) {
            const RinImportV3* imported = &imports[import_index];
            uintptr_t address;
            if (imported->name_offset >= header->string_table_size ||
                imported->kind != RIN_SYMBOL_FUNCTION ||
                imported->dependency_index >= header->dependency_count ||
                !range_valid(imported->target_rva, pointer_width,
                             header->image_size)) {
                fail("invalid bootstrap import");
            }
            address = symbol_address(strings + imported->name_offset);
            if (pointer_width == 4u) {
                uint32_t value;
                if (address > UINT32_MAX) fail("i686 import overflow");
                value = (uint32_t)address;
                memcpy(image + imported->target_rva, &value, 4u);
            } else {
                uint64_t value = (uint64_t)address;
                memcpy(image + imported->target_rva, &value, 8u);
            }
        }
    }
}

static void enforce_wx(uint8_t* image, const RinHeaderV3* header,
                       const RinSectionV3* sections)
{
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        if (section->memory_size == 0u) continue;
        if (section->type == RIN_IMAGE_SECTION_CODE) {
            protect_range(image, header->image_size,
                          section->virtual_address, section->memory_size,
                          PROT_READ | PROT_EXEC);
        } else if (section->type == RIN_IMAGE_SECTION_DATA ||
                   section->type == RIN_IMAGE_SECTION_BSS) {
            protect_range(image, header->image_size,
                          section->virtual_address, section->memory_size,
                          PROT_READ | PROT_WRITE);
        }
    }
}

int main(int argc, char** argv)
{
    FILE* file;
    long end;
    uint8_t* bytes;
    RinHeaderV3* header;
    RinSectionV3* sections;
    uint8_t* image;
    RccEntry entry;
    void* entry_address;
    int result;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s IMAGE TOOL-NAME [RCC-ARGUMENT ...]\n", argv[0]);
        return 2;
    }
    file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0 ||
        (end = ftell(file)) < (long)sizeof(RinHeaderV3) ||
        fseek(file, 0, SEEK_SET) != 0) {
        fail("cannot read image");
    }
    bytes = malloc((size_t)end);
    if (!bytes || fread(bytes, 1u, (size_t)end, file) != (size_t)end ||
        fclose(file) != 0) {
        fail("cannot load image bytes");
    }
    header = (RinHeaderV3*)bytes;
    if (header->magic != RIN_IMAGE_MAGIC ||
        header->version != RIN_IMAGE_VERSION_3 ||
        header->header_size != sizeof(*header) ||
        !range_valid(header->section_table_offset,
                     (uint64_t)header->section_count * sizeof(RinSectionV3),
                     (uint64_t)end) ||
        header->entry_rva >= header->image_size ||
        header->image_size == 0u || header->image_size > SIZE_MAX) {
        fail("invalid RIN v3 header");
    }
#if defined(__i386__)
    if (header->architecture != RIN_ARCH_X86) fail("runner architecture mismatch");
#else
    if (header->architecture != RIN_ARCH_X86_64) fail("runner architecture mismatch");
#endif
    sections = (RinSectionV3*)(bytes + header->section_table_offset);
    image = map_image(bytes, (size_t)end, header, sections);
    apply_relocations(image, bytes, (size_t)end, header, sections);
    bind_imports(image, bytes, (size_t)end, header, sections);
    enforce_wx(image, header, sections);
    entry_address = image + header->entry_rva;
    memcpy(&entry, &entry_address, sizeof(entry));
    result = entry(argc - 2, &argv[2]);
    if (result != 0) {
        fprintf(stderr, "bootstrap-stage-runner: stage returned %d\n", result);
    }
    /* Keep the image mapped: stage1 registers generated atexit callbacks. */
    free(bytes);
    return result;
}
