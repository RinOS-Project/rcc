#define _GNU_SOURCE

#include "objfile.h"

#include <assert.h>
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN64)
#define RINOS_ABI __attribute__((sysv_abi))
#else
#define RINOS_ABI
#endif

typedef int RINOS_ABI (*EntryFunction)(void);
typedef long long RINOS_ABI (*WideLoopFunction)(unsigned int, long long);
typedef long long RINOS_ABI (*WideRestartFunction)(
    unsigned int, unsigned int, unsigned int, long long);
typedef float RINOS_ABI (*FloatUnaryFunction)(float);
typedef double RINOS_ABI (*DoubleUnaryFunction)(double);
typedef void RINOS_ABI (*DiscardDoubleFunction)(double);

static int object_section_index(const ObjectFile* object,
                                const ObjSection* wanted)
{
    const ObjSection* section;
    int index = 0;
    for (section = object ? object->sections : NULL;
         section; section = section->next, ++index) {
        if (section == wanted) return index;
    }
    return -1;
}

int main(int argc, char** argv)
{
    int entry_only;
    int rint_only;
    ObjectFile* object;
    ObjSection* text;
    ObjSection* rodata;
    ObjSection* data;
    ObjSection* bss;
    int rodata_index;
    int data_index;
    int bss_index;
    ObjSymbol* entry_symbol;
    ObjSymbol* wide_loop_symbol = NULL;
    ObjSymbol* nested_wide_loop_symbol = NULL;
    ObjSymbol* nested_wide_restart_symbol = NULL;
    ObjSymbol* rintf_symbol = NULL;
    ObjSymbol* rint_symbol = NULL;
    ObjSymbol* rintl_symbol = NULL;
    ObjSymbol* rint_discard_symbol = NULL;
    size_t page_size;
    size_t mapping_size;
    size_t text_mapping_size;
    size_t rodata_offset = 0u;
    size_t data_offset = 0u;
    size_t bss_offset = 0u;
    size_t used_size;
    void* memory;
    void* entry_address;
    EntryFunction entry;
    WideLoopFunction wide_loop;
    WideLoopFunction nested_wide_loop;
    WideRestartFunction nested_wide_restart;
    FloatUnaryFunction rintf_function;
    DoubleUnaryFunction rint_function;
    DoubleUnaryFunction rintl_function;
    DiscardDoubleFunction rint_discard_function;
#ifdef _WIN32
    SYSTEM_INFO system_info;
    DWORD previous;
#else
    long host_page_size;
#endif

    entry_only = argc == 3 && strcmp(argv[1], "--entry") == 0;
    rint_only = argc == 3 && strcmp(argv[1], "--rint") == 0;
    assert(argc == 2 || entry_only || rint_only);
    object = objfile_read((entry_only || rint_only) ? argv[2] : argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    rodata = objfile_get_section(object, ".rodata");
    data = objfile_get_section(object, ".data");
    bss = objfile_get_section(object, ".bss");
    rodata_index = object_section_index(object, rodata);
    data_index = object_section_index(object, data);
    bss_index = object_section_index(object, bss);
    entry_symbol = rint_only ? NULL : objfile_find_symbol(object, "main");
    assert(text != NULL && text->data != NULL && text->size != 0u &&
           (rint_only ||
            (entry_symbol != NULL && entry_symbol->section == 0)));
    if (rint_only) {
        rintf_symbol = objfile_find_symbol(object, "verified_rintf");
        rint_symbol = objfile_find_symbol(object, "verified_rint");
        rintl_symbol = objfile_find_symbol(
            object, "verified_rintl_as_double");
        rint_discard_symbol = objfile_find_symbol(
            object, "verified_rint_discard");
        assert(rintf_symbol != NULL && rintf_symbol->section == 0 &&
               rint_symbol != NULL && rint_symbol->section == 0 &&
               rintl_symbol != NULL && rintl_symbol->section == 0 &&
               rint_discard_symbol != NULL &&
               rint_discard_symbol->section == 0);
    }
    if (!entry_only && !rint_only) {
        wide_loop_symbol = objfile_find_symbol(
            object, "verified_goto_wide_inside_loop");
        nested_wide_loop_symbol = objfile_find_symbol(
            object, "verified_goto_wide_nested_loop");
        nested_wide_restart_symbol = objfile_find_symbol(
            object, "verified_goto_wide_nested_backward_restart");
        assert(wide_loop_symbol != NULL && wide_loop_symbol->section == 0 &&
               nested_wide_loop_symbol != NULL &&
               nested_wide_loop_symbol->section == 0 &&
               nested_wide_restart_symbol != NULL &&
               nested_wide_restart_symbol->section == 0);
    }
#ifdef _WIN32
    GetSystemInfo(&system_info);
    page_size = (size_t)system_info.dwPageSize;
#else
    host_page_size = sysconf(_SC_PAGESIZE);
    page_size = host_page_size > 0 ? (size_t)host_page_size : 0u;
#endif
    assert(page_size != 0u && text->size <= SIZE_MAX - page_size + 1u);
    text_mapping_size = ((size_t)text->size + page_size - 1u) &
        ~(page_size - 1u);
    used_size = text_mapping_size;
    if (rodata && rodata->size != 0u) {
        size_t alignment = rodata->align ? rodata->align : 1u;
        if (alignment < page_size) alignment = page_size;
        size_t mask = alignment - 1u;
        assert((alignment & mask) == 0u && used_size <= SIZE_MAX - mask);
        rodata_offset = (used_size + mask) & ~mask;
        assert(rodata->size <= SIZE_MAX - rodata_offset);
        used_size = rodata_offset + (size_t)rodata->size;
    }
    if (data && data->size != 0u) {
        size_t alignment = data->align ? data->align : 1u;
        if (alignment < page_size) alignment = page_size;
        size_t mask = alignment - 1u;
        assert((alignment & mask) == 0u && used_size <= SIZE_MAX - mask);
        data_offset = (used_size + mask) & ~mask;
        assert(data->size <= SIZE_MAX - data_offset);
        used_size = data_offset + (size_t)data->size;
    }
    if (bss && bss->memory_size != 0u) {
        size_t alignment = bss->align ? bss->align : 1u;
        if (alignment < page_size) alignment = page_size;
        size_t mask = alignment - 1u;
        assert((alignment & mask) == 0u && used_size <= SIZE_MAX - mask);
        bss_offset = (used_size + mask) & ~mask;
        assert(bss->memory_size <= SIZE_MAX - bss_offset);
        used_size = bss_offset + (size_t)bss->memory_size;
    }
    assert(used_size <= SIZE_MAX - page_size + 1u);
    mapping_size = (used_size + page_size - 1u) & ~(page_size - 1u);
#ifdef _WIN32
    memory = VirtualAlloc(NULL, mapping_size, MEM_RESERVE | MEM_COMMIT,
                          PAGE_READWRITE);
#else
    memory = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) memory = NULL;
#endif
    assert(memory != NULL);
    memcpy(memory, text->data, (size_t)text->size);
    if (rodata && rodata->size != 0u) {
        memcpy((uint8_t*)memory + rodata_offset,
               rodata->data, (size_t)rodata->size);
    }
    if (data && data->size != 0u) {
        memcpy((uint8_t*)memory + data_offset,
               data->data, (size_t)data->size);
    }
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        ObjSymbol* symbol = objfile_find_symbol(
            object, relocation->symbol_name);
        uint8_t* place;
        size_t symbol_section_offset;
        int64_t target;
        int64_t delta;
        int32_t encoded;
        assert(symbol != NULL && relocation->offset <= text->size);
        if (symbol->section == 0) {
            symbol_section_offset = 0u;
        } else if (rodata && symbol->section == rodata_index) {
            symbol_section_offset = rodata_offset;
        } else if (data && symbol->section == data_index) {
            symbol_section_offset = data_offset;
        } else if (bss && symbol->section == bss_index) {
            symbol_section_offset = bss_offset;
        } else {
            assert(!"entry runner relocation target section is unsupported");
            symbol_section_offset = 0u;
        }
        place = (uint8_t*)memory + relocation->offset;
        target = (int64_t)(uintptr_t)memory +
            (int64_t)symbol_section_offset +
            (int64_t)symbol->value + relocation->addend;
        if (relocation->type == RELOC_REL32) {
            assert(sizeof(encoded) <= text->size - relocation->offset);
            delta = target -
                (int64_t)(uintptr_t)(place + sizeof(encoded));
            encoded = (int32_t)delta;
            assert((int64_t)encoded == delta);
            memcpy(place, &encoded, sizeof(encoded));
        } else if (relocation->type == RELOC_ABS64) {
            uint64_t encoded_address = (uint64_t)target;
            assert(sizeof(encoded_address) <=
                   text->size - relocation->offset);
            memcpy(place, &encoded_address, sizeof(encoded_address));
        } else {
            assert(!"entry runner does not support this relocation type");
        }
    }
#ifdef _WIN32
    assert(VirtualProtect(memory, text_mapping_size,
                          PAGE_EXECUTE_READ, &previous));
#else
    assert(mprotect(memory, text_mapping_size,
                    PROT_READ | PROT_EXEC) == 0);
#endif
    if (!rint_only) {
        entry_address = (uint8_t*)memory + entry_symbol->value;
        memcpy(&entry, &entry_address, sizeof(entry));
        assert(entry() == 0);
    }
    if (entry_only) {
#ifdef _WIN32
        assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
        assert(munmap(memory, mapping_size) == 0);
#endif
        objfile_free(object);
        puts("RCC object entry execution passed");
        return 0;
    }
    if (rint_only) {
        static const int modes[] = {
            FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO
        };
        static const float positive_f[] = {2.0f, 2.0f, 3.0f, 2.0f};
        static const float negative_f[] = {-2.0f, -3.0f, -2.0f, -2.0f};
        static const double positive[] = {2.0, 2.0, 3.0, 2.0};
        static const double negative[] = {-2.0, -3.0, -2.0, -2.0};
        int original_mode = fegetround();
        assert(original_mode != -1);
        entry_address = (uint8_t*)memory + rintf_symbol->value;
        memcpy(&rintf_function, &entry_address, sizeof(rintf_function));
        entry_address = (uint8_t*)memory + rint_symbol->value;
        memcpy(&rint_function, &entry_address, sizeof(rint_function));
        entry_address = (uint8_t*)memory + rintl_symbol->value;
        memcpy(&rintl_function, &entry_address, sizeof(rintl_function));
        entry_address = (uint8_t*)memory + rint_discard_symbol->value;
        memcpy(&rint_discard_function, &entry_address,
               sizeof(rint_discard_function));
        for (size_t index = 0u;
             index < sizeof(modes) / sizeof(modes[0]); ++index) {
            assert(fesetround(modes[index]) == 0);
            assert(rintf_function(2.5f) == positive_f[index]);
            assert(rintf_function(-2.5f) == negative_f[index]);
            assert(rint_function(2.5) == positive[index]);
            assert(rint_function(-2.5) == negative[index]);
            assert(rintl_function(2.5) == positive[index]);
            assert(rintl_function(-2.5) == negative[index]);
        }
        assert(fesetround(FE_TONEAREST) == 0);
        {
            uint64_t bits;
            double negative_zero = rint_function(-0.25);
            memcpy(&bits, &negative_zero, sizeof(bits));
            assert(bits == UINT64_C(0x8000000000000000));
        }
        assert(feclearexcept(FE_ALL_EXCEPT) == 0);
        rint_discard_function(1.25);
        assert((fetestexcept(FE_INEXACT) & FE_INEXACT) != 0);
        assert(fesetround(original_mode) == 0);
#ifdef _WIN32
        assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
        assert(munmap(memory, mapping_size) == 0);
#endif
        objfile_free(object);
        puts("Verified backend dynamic rint execution passed");
        return 0;
    }
    entry_address = (uint8_t*)memory + wide_loop_symbol->value;
    memcpy(&wide_loop, &entry_address, sizeof(wide_loop));
    assert(wide_loop(0u, 0x0000000200000002LL) ==
           0x0000000200000002LL);
    assert(wide_loop(3u, 0x0000000200000002LL) ==
           0x000000060000000cLL);
    entry_address = (uint8_t*)memory + nested_wide_loop_symbol->value;
    memcpy(&nested_wide_loop, &entry_address, sizeof(nested_wide_loop));
    assert(nested_wide_loop(0u, 0x0000000200000002LL) ==
           0x0000000200000002LL);
    assert(nested_wide_loop(1u, 0x0000000200000002LL) ==
           0x0000000600000010LL);
    assert(nested_wide_loop(3u, 0x0000000200000002LL) ==
           0x0000000e00000038LL);
    entry_address = (uint8_t*)memory + nested_wide_restart_symbol->value;
    memcpy(&nested_wide_restart, &entry_address, sizeof(nested_wide_restart));
    assert(nested_wide_restart(2u, 2u, 2u, 7) == 34359738386LL);
    assert(nested_wide_restart(1u, 0u, 2u, 7) == 8);
#ifdef _WIN32
    assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
    assert(munmap(memory, mapping_size) == 0);
#endif
    objfile_free(object);
    puts("Verified backend goto/label execution passed");
    return 0;
}
