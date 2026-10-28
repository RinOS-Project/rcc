#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef float _Complex RccComplexFloat;
typedef double _Complex RccComplexDouble;
extern void rcc_host_complex_mul_f32(float, float, float, float,
                                     float*, float*);
extern void rcc_host_complex_mul_f64(double, double, double, double,
                                     double*, double*);
extern void rcc_host_complex_div_f32(float, float, float, float,
                                     float*, float*);
extern void rcc_host_complex_div_f64(double, double, double, double,
                                     double*, double*);

/* The loaded RCC code follows the target SysV ABI.  Bridge to the host C
 * runtime's calling convention while delegating arithmetic to RinOS libc's
 * real complex helpers, not test-only replacement arithmetic. */
void RINOS_ABI __rin_cmul_f32(float a, float b, float c, float d,
                             float* real, float* imaginary) {
    rcc_host_complex_mul_f32(a, b, c, d, real, imaginary);
}

void RINOS_ABI __rin_cmul_f64(double a, double b, double c, double d,
                             double* real, double* imaginary) {
    rcc_host_complex_mul_f64(a, b, c, d, real, imaginary);
}

void RINOS_ABI __rin_cdiv_f32(float a, float b, float c, float d,
                             float* real, float* imaginary) {
    rcc_host_complex_div_f32(a, b, c, d, real, imaginary);
}

void RINOS_ABI __rin_cdiv_f64(double a, double b, double c, double d,
                             double* real, double* imaginary) {
    rcc_host_complex_div_f64(a, b, c, d, real, imaginary);
}

typedef RccComplexFloat RINOS_ABI (*ComplexFloatUnary)(RccComplexFloat);
typedef RccComplexFloat RINOS_ABI (*ComplexFloatBinary)(RccComplexFloat,
                                                        RccComplexFloat);
typedef RccComplexDouble RINOS_ABI (*ComplexDoubleUnary)(RccComplexDouble);
typedef RccComplexDouble RINOS_ABI (*ComplexDoubleBinary)(RccComplexDouble,
                                                          RccComplexDouble);
typedef float RINOS_ABI (*ComplexFloatReal)(RccComplexFloat);
typedef int RINOS_ABI (*ComplexFloatTruth)(RccComplexFloat);
typedef int RINOS_ABI (*ComplexFloatEqual)(RccComplexFloat,
                                           RccComplexFloat);
typedef int RINOS_ABI (*ComplexLocalTest)(void);
typedef RccComplexDouble RINOS_ABI (*ComplexFloatToDouble)(
    RccComplexFloat);
typedef RccComplexDouble RINOS_ABI (*ComplexDoubleNoArg)(void);

typedef struct MappedObject {
    void* memory;
    size_t size;
    size_t* section_offsets;
} MappedObject;

typedef union ComplexRuntimeAddress {
    void* object;
    void (RINOS_ABI *f32)(float, float, float, float, float*, float*);
    void (RINOS_ABI *f64)(double, double, double, double, double*, double*);
} ComplexRuntimeAddress;

static void* complex_runtime_import(const char* name) {
    ComplexRuntimeAddress address = {0};
    if (strcmp(name, "__rin_cmul_f32") == 0) {
        address.f32 = __rin_cmul_f32;
    } else if (strcmp(name, "__rin_cmul_f64") == 0) {
        address.f64 = __rin_cmul_f64;
    } else if (strcmp(name, "__rin_cdiv_f32") == 0) {
        address.f32 = __rin_cdiv_f32;
    } else if (strcmp(name, "__rin_cdiv_f64") == 0) {
        address.f64 = __rin_cdiv_f64;
    } else {
        return NULL;
    }
    return address.object;
}

static MappedObject map_object(ObjectFile* object) {
    size_t page;
    size_t size = 0u;
    size_t section_end;
    size_t relocation_count = 0u;
    size_t thunk_cursor;
    size_t section_index = 0u;
    MappedObject mapped = {0};
#ifdef _WIN32
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    page = (size_t)system_info.dwPageSize;
#else
    long host_page = sysconf(_SC_PAGESIZE);
    page = host_page > 0 ? (size_t)host_page : 0u;
#endif
    assert(object != NULL && object->section_count > 0 && page != 0u);
    mapped.section_offsets = calloc((size_t)object->section_count,
                                    sizeof(*mapped.section_offsets));
    assert(mapped.section_offsets != NULL);
    for (ObjSection* section = object->sections; section;
         section = section->next, ++section_index) {
        size_t alignment;
        assert(section_index < (size_t)object->section_count);
        mapped.section_offsets[section_index] = SIZE_MAX;
        if ((section->flags & SECT_FLAG_ALLOC) == 0u) continue;
        alignment = section->align > 1u ? (size_t)section->align : 1u;
        assert((alignment & (alignment - 1u)) == 0u &&
               size <= SIZE_MAX - (alignment - 1u));
        size = (size + alignment - 1u) & ~(alignment - 1u);
        mapped.section_offsets[section_index] = size;
        assert(section->memory_size <= SIZE_MAX - size);
        size += (size_t)section->memory_size;
        for (ObjReloc* relocation = section->relocs; relocation;
             relocation = relocation->next) {
            assert(relocation_count < SIZE_MAX / 16u);
            ++relocation_count;
        }
    }
    section_end = size;
    assert(section_index == (size_t)object->section_count && size != 0u &&
           relocation_count <= (SIZE_MAX - size) / 16u);
    size += relocation_count * 16u;
    assert(size <= SIZE_MAX - page + 1u);
    mapped.size = (size + page - 1u) & ~(page - 1u);
    thunk_cursor = (section_end + 15u) & ~(size_t)15u;
#ifdef _WIN32
    mapped.memory = VirtualAlloc(NULL, mapped.size, MEM_RESERVE | MEM_COMMIT,
                                 PAGE_READWRITE);
#else
    mapped.memory = mmap(NULL, mapped.size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped.memory == MAP_FAILED) mapped.memory = NULL;
#endif
    assert(mapped.memory != NULL);
    section_index = 0u;
    for (ObjSection* section = object->sections; section;
         section = section->next, ++section_index) {
        if (mapped.section_offsets[section_index] == SIZE_MAX) continue;
        assert(section->size <= section->memory_size);
        if (section->size != 0u) {
            assert(section->data != NULL);
            memcpy((uint8_t*)mapped.memory +
                       mapped.section_offsets[section_index],
                   section->data, (size_t)section->size);
        }
    }
    section_index = 0u;
    for (ObjSection* section = object->sections; section;
         section = section->next, ++section_index) {
        if (mapped.section_offsets[section_index] == SIZE_MAX) continue;
        for (ObjReloc* relocation = section->relocs; relocation;
             relocation = relocation->next) {
            ObjSymbol* symbol = objfile_find_symbol(
                object, relocation->symbol_name);
            uint8_t* location;
            uint64_t symbol_value;
            int64_t relative_value;
            if (!symbol) {
                fprintf(stderr, "missing test relocation symbol: %s\n",
                        relocation->symbol_name);
            }
            assert(symbol != NULL);
            if (symbol->section < 0) {
                void* imported = complex_runtime_import(
                    relocation->symbol_name);
                uint8_t* thunk;
                assert(imported != NULL &&
                       thunk_cursor <= mapped.size - 16u);
                thunk = (uint8_t*)mapped.memory + thunk_cursor;
#if defined(__x86_64__) || defined(_M_X64)
                thunk[0] = 0x48u;
                thunk[1] = 0xb8u;
                memcpy(thunk + 2u, &imported, sizeof(imported));
                thunk[10] = 0xffu;
                thunk[11] = 0xe0u;
#else
                {
                    uint32_t target = (uint32_t)(uintptr_t)imported;
                    thunk[0] = 0xb8u;
                    memcpy(thunk + 1u, &target, sizeof(target));
                    thunk[5] = 0xffu;
                    thunk[6] = 0xe0u;
                }
#endif
                symbol_value = (uint64_t)(uintptr_t)thunk;
                thunk_cursor += 16u;
            } else {
                assert((size_t)symbol->section <
                           (size_t)object->section_count &&
                       mapped.section_offsets[symbol->section] != SIZE_MAX &&
                       symbol->value <= SIZE_MAX -
                           mapped.section_offsets[symbol->section]);
                symbol_value = (uint64_t)(uintptr_t)mapped.memory +
                    mapped.section_offsets[symbol->section] + symbol->value;
            }
            location = (uint8_t*)mapped.memory +
                mapped.section_offsets[section_index] + relocation->offset;
            switch (relocation->type) {
                case RELOC_ABS64: {
                    uint64_t value = symbol_value +
                        (uint64_t)relocation->addend;
                    memcpy(location, &value, sizeof(value));
                    break;
                }
                case RELOC_ABS32:
                case RELOC_ABS32U:
                case RELOC_ABS32S: {
                    uint32_t value = (uint32_t)(symbol_value +
                        (uint64_t)relocation->addend);
                    memcpy(location, &value, sizeof(value));
                    break;
                }
                case RELOC_REL32:
                    relative_value = (int64_t)symbol_value +
                        relocation->addend -
                        ((int64_t)(uintptr_t)location + 4);
                    assert(relative_value >= INT32_MIN &&
                           relative_value <= INT32_MAX);
                    {
                        int32_t value = (int32_t)relative_value;
                        memcpy(location, &value, sizeof(value));
                    }
                    break;
                case RELOC_REL8:
                    relative_value = (int64_t)symbol_value +
                        relocation->addend -
                        ((int64_t)(uintptr_t)location + 1);
                    assert(relative_value >= INT8_MIN &&
                           relative_value <= INT8_MAX);
                    *location = (uint8_t)(int8_t)relative_value;
                    break;
                default:
                    assert(!"complex test object has an unsupported relocation");
            }
        }
    }
#ifdef _WIN32
    {
        DWORD previous;
        assert(VirtualProtect(mapped.memory, mapped.size,
                              PAGE_EXECUTE_READWRITE, &previous));
    }
#else
    assert(mprotect(mapped.memory, mapped.size,
                    PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
#endif
    return mapped;
}

static void unmap_object(MappedObject mapped) {
#ifdef _WIN32
    assert(VirtualFree(mapped.memory, 0, MEM_RELEASE));
#else
    assert(munmap(mapped.memory, mapped.size) == 0);
#endif
    free(mapped.section_offsets);
}

static int close_float(float actual, float expected) {
    float delta = actual - expected;
    return delta > -0.0001f && delta < 0.0001f;
}

static int close_double(double actual, double expected) {
    double delta = actual - expected;
    return delta > -0.0000000001 && delta < 0.0000000001;
}

static void* symbol_address(MappedObject mapped,
                            const ObjectFile* object, const char* name) {
    ObjSymbol* symbol = objfile_find_symbol((ObjectFile*)object, name);
    ObjSection* section;
    assert(symbol != NULL && symbol->section >= 0 &&
           (size_t)symbol->section < (size_t)object->section_count);
    section = object->sections;
    for (int index = 0; index < symbol->section; ++index) {
        assert(section != NULL);
        section = section->next;
    }
    assert(section != NULL &&
           mapped.section_offsets[symbol->section] != SIZE_MAX &&
           symbol->value < section->memory_size);
    return (uint8_t*)mapped.memory +
        mapped.section_offsets[symbol->section] + symbol->value;
}

int main(int argc, char** argv) {
    ObjectFile* object;
    MappedObject mapped;
    void* address;
    ComplexFloatUnary float_identity;
    ComplexFloatBinary float_add;
    ComplexFloatBinary float_multiply;
    ComplexFloatBinary float_divide;
    ComplexDoubleUnary double_identity;
    ComplexDoubleBinary double_add;
    ComplexDoubleBinary double_multiply;
    ComplexDoubleBinary double_divide;
    ComplexFloatUnary float_negate;
    ComplexFloatReal float_real;
    ComplexFloatTruth float_truth;
    ComplexFloatEqual float_equal;
    ComplexFloatToDouble float_to_double;
    ComplexDoubleNoArg standard_imaginary_unit;
    ComplexDoubleNoArg standard_constructor;
    ComplexLocalTest local_scalar_initializer;
    ComplexLocalTest local_constructor_initializer;
    ComplexLocalTest local_arithmetic;
    RccComplexFloat a = __builtin_complex(2.0f, 3.0f);
    RccComplexFloat b = __builtin_complex(4.0f, -1.0f);
    RccComplexFloat value_float;
    RccComplexDouble x = __builtin_complex(2.0, 3.0);
    RccComplexDouble y = __builtin_complex(4.0, -1.0);
    RccComplexDouble value_double;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    mapped = map_object(object);

    address = symbol_address(mapped, object,
                             "rcc_complex_float_identity");
    memcpy(&float_identity, &address, sizeof(float_identity));
    address = symbol_address(mapped, object, "rcc_complex_float_add");
    memcpy(&float_add, &address, sizeof(float_add));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_multiply");
    memcpy(&float_multiply, &address, sizeof(float_multiply));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_divide");
    memcpy(&float_divide, &address, sizeof(float_divide));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_negate");
    memcpy(&float_negate, &address, sizeof(float_negate));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_real");
    memcpy(&float_real, &address, sizeof(float_real));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_truth");
    memcpy(&float_truth, &address, sizeof(float_truth));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_equal");
    memcpy(&float_equal, &address, sizeof(float_equal));
    address = symbol_address(mapped, object,
                             "rcc_complex_float_to_double");
    memcpy(&float_to_double, &address, sizeof(float_to_double));
    address = symbol_address(mapped, object,
                             "rcc_complex_standard_imaginary_unit");
    memcpy(&standard_imaginary_unit, &address,
           sizeof(standard_imaginary_unit));
    address = symbol_address(mapped, object,
                             "rcc_complex_standard_constructor");
    memcpy(&standard_constructor, &address, sizeof(standard_constructor));
    address = symbol_address(mapped, object,
                             "rcc_complex_local_scalar_initializer");
    memcpy(&local_scalar_initializer, &address,
           sizeof(local_scalar_initializer));
    address = symbol_address(mapped, object,
                             "rcc_complex_local_constructor_initializer");
    memcpy(&local_constructor_initializer, &address,
           sizeof(local_constructor_initializer));
    address = symbol_address(mapped, object,
                             "rcc_complex_local_arithmetic");
    memcpy(&local_arithmetic, &address, sizeof(local_arithmetic));
    address = symbol_address(mapped, object,
                             "rcc_complex_double_identity");
    memcpy(&double_identity, &address, sizeof(double_identity));
    address = symbol_address(mapped, object, "rcc_complex_double_add");
    memcpy(&double_add, &address, sizeof(double_add));
    address = symbol_address(mapped, object,
                             "rcc_complex_double_multiply");
    memcpy(&double_multiply, &address, sizeof(double_multiply));
    address = symbol_address(mapped, object,
                             "rcc_complex_double_divide");
    memcpy(&double_divide, &address, sizeof(double_divide));

    value_float = float_identity(a);
    assert(close_float(__real__ value_float, 2.0f) &&
           close_float(__imag__ value_float, 3.0f));
    value_float = float_add(a, b);
    assert(close_float(__real__ value_float, 6.0f) &&
           close_float(__imag__ value_float, 2.0f));
    value_float = float_multiply(a, b);
    assert(close_float(__real__ value_float, 11.0f) &&
           close_float(__imag__ value_float, 10.0f));
    value_float = float_divide(a, __builtin_complex(1.0f, 1.0f));
    assert(close_float(__real__ value_float, 2.5f) &&
           close_float(__imag__ value_float, 0.5f));
    value_float = float_negate(a);
    assert(close_float(__real__ value_float, -2.0f) &&
           close_float(__imag__ value_float, -3.0f));
    assert(close_float(float_real(a), 2.0f));
    assert(float_truth(a) == 1 && float_truth(__builtin_complex(0.0f, 0.0f)) == 0);
    assert(float_equal(a, __builtin_complex(2.0f, 3.0f)) == 1);
    assert(float_equal(a, b) == 0);
    value_double = float_to_double(a);
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, 3.0));
    value_double = standard_imaginary_unit();
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, 3.0));
    value_double = standard_constructor();
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, 3.0));
    assert(local_scalar_initializer() == 0);
    assert(local_constructor_initializer() == 0);
    assert(local_arithmetic() == 0);

    value_double = double_identity(x);
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, 3.0));
    value_double = double_add(x, y);
    assert(close_double(__real__ value_double, 6.0) &&
           close_double(__imag__ value_double, 2.0));
    value_double = double_multiply(x, y);
    assert(close_double(__real__ value_double, 11.0) &&
           close_double(__imag__ value_double, 10.0));
    value_double = double_divide(x, __builtin_complex(1.0, 1.0));
    assert(close_double(__real__ value_double, 2.5) &&
           close_double(__imag__ value_double, 0.5));

    unmap_object(mapped);
    objfile_free(object);
    puts("RCC complex arithmetic and SysV ABI execution passed");
    return 0;
}
