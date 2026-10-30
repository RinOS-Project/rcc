#include "objfile.h"

#include <assert.h>
#include <fenv.h>
#include <math.h>
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
typedef long double _Complex RccHostComplexLongDouble;
extern long double creall(RccHostComplexLongDouble);
extern long double cimagl(RccHostComplexLongDouble);
extern long double cabsl(RccHostComplexLongDouble);
extern long double cargl(RccHostComplexLongDouble);
extern RccHostComplexLongDouble conjl(RccHostComplexLongDouble);
extern RccHostComplexLongDouble csqrtl(RccHostComplexLongDouble);
extern float crealf(RccComplexFloat);
extern float cimagf(RccComplexFloat);
extern float cabsf(RccComplexFloat);
extern float cargf(RccComplexFloat);
extern RccComplexFloat conjf(RccComplexFloat);
extern RccComplexFloat csqrtf(RccComplexFloat);
extern double creal(RccComplexDouble);
extern double cimag(RccComplexDouble);
extern double cabs(RccComplexDouble);
extern double carg(RccComplexDouble);
extern RccComplexDouble conj(RccComplexDouble);
extern RccComplexDouble csqrt(RccComplexDouble);
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
typedef float RINOS_ABI (*ComplexFloatToReal)(RccComplexFloat);
typedef double RINOS_ABI (*ComplexDoubleToReal)(RccComplexDouble);
typedef float RINOS_ABI (*ComplexFloatReal)(RccComplexFloat);
typedef int RINOS_ABI (*ComplexFloatTruth)(RccComplexFloat);
typedef int RINOS_ABI (*ComplexFloatEqual)(RccComplexFloat,
                                           RccComplexFloat);
typedef int RINOS_ABI (*ComplexLocalTest)(void);
typedef RccComplexDouble RINOS_ABI (*ComplexFloatToDouble)(
    RccComplexFloat);
typedef RccComplexDouble RINOS_ABI (*ComplexDoubleNoArg)(void);

/* RinOS long double uses the binary64 ABI.  Keep the host harness independent
 * of the host compiler's long-double format while matching its two-SSE-word
 * SysV aggregate calling convention. */
typedef struct ComplexBinary64Pair {
    double real;
    double imaginary;
} ComplexBinary64Pair;
typedef ComplexBinary64Pair RINOS_ABI (*ComplexPairUnary)(
    ComplexBinary64Pair);
typedef ComplexBinary64Pair RINOS_ABI (*ComplexPairBinary)(
    ComplexBinary64Pair, ComplexBinary64Pair);
typedef double RINOS_ABI (*ComplexLongDoubleToReal)(ComplexBinary64Pair);
typedef ComplexBinary64Pair RINOS_ABI (*ComplexLongDoubleUnary)(
    ComplexBinary64Pair);

static RccHostComplexLongDouble complex_pair_to_host(
    ComplexBinary64Pair value) {
    return __builtin_complex((long double)value.real,
                             (long double)value.imaginary);
}

static ComplexBinary64Pair complex_host_to_pair(
    RccHostComplexLongDouble value) {
    ComplexBinary64Pair result;
    result.real = (double)__real__ value;
    result.imaginary = (double)__imag__ value;
    return result;
}

static double RINOS_ABI complex_import_creall(ComplexBinary64Pair value) {
    return (double)creall(complex_pair_to_host(value));
}

static double RINOS_ABI complex_import_cimagl(ComplexBinary64Pair value) {
    return (double)cimagl(complex_pair_to_host(value));
}

static double RINOS_ABI complex_import_cabsl(ComplexBinary64Pair value) {
    return (double)cabsl(complex_pair_to_host(value));
}

static double RINOS_ABI complex_import_cargl(ComplexBinary64Pair value) {
    return (double)cargl(complex_pair_to_host(value));
}

static ComplexBinary64Pair RINOS_ABI complex_import_conjl(
    ComplexBinary64Pair value) {
    return complex_host_to_pair(conjl(complex_pair_to_host(value)));
}

static ComplexBinary64Pair RINOS_ABI complex_import_csqrtl(
    ComplexBinary64Pair value) {
    return complex_host_to_pair(csqrtl(complex_pair_to_host(value)));
}

typedef struct MappedObject {
    void* memory;
    size_t size;
    size_t* section_offsets;
} MappedObject;

typedef union ComplexRuntimeAddress {
    void* object;
    void (RINOS_ABI *f32)(float, float, float, float, float*, float*);
    void (RINOS_ABI *f64)(double, double, double, double, double*, double*);
    float (RINOS_ABI *complex_float_real)(RccComplexFloat);
    RccComplexFloat (RINOS_ABI *complex_float_unary)(RccComplexFloat);
    double (RINOS_ABI *complex_double_real)(RccComplexDouble);
    RccComplexDouble (RINOS_ABI *complex_double_unary)(RccComplexDouble);
    double (RINOS_ABI *complex_long_double_real)(ComplexBinary64Pair);
    ComplexBinary64Pair (RINOS_ABI *complex_long_double_unary)(
        ComplexBinary64Pair);
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
    } else if (strcmp(name, "crealf") == 0) {
        address.complex_float_real = crealf;
    } else if (strcmp(name, "cimagf") == 0) {
        address.complex_float_real = cimagf;
    } else if (strcmp(name, "cabsf") == 0) {
        address.complex_float_real = cabsf;
    } else if (strcmp(name, "cargf") == 0) {
        address.complex_float_real = cargf;
    } else if (strcmp(name, "conjf") == 0) {
        address.complex_float_unary = conjf;
    } else if (strcmp(name, "csqrtf") == 0) {
        address.complex_float_unary = csqrtf;
    } else if (strcmp(name, "creal") == 0) {
        address.complex_double_real = creal;
    } else if (strcmp(name, "cimag") == 0) {
        address.complex_double_real = cimag;
    } else if (strcmp(name, "cabs") == 0) {
        address.complex_double_real = cabs;
    } else if (strcmp(name, "carg") == 0) {
        address.complex_double_real = carg;
    } else if (strcmp(name, "conj") == 0) {
        address.complex_double_unary = conj;
    } else if (strcmp(name, "csqrt") == 0) {
        address.complex_double_unary = csqrt;
    } else if (strcmp(name, "creall") == 0) {
        address.complex_long_double_real = complex_import_creall;
    } else if (strcmp(name, "cimagl") == 0) {
        address.complex_long_double_real = complex_import_cimagl;
    } else if (strcmp(name, "cabsl") == 0) {
        address.complex_long_double_real = complex_import_cabsl;
    } else if (strcmp(name, "cargl") == 0) {
        address.complex_long_double_real = complex_import_cargl;
    } else if (strcmp(name, "conjl") == 0) {
        address.complex_long_double_unary = complex_import_conjl;
    } else if (strcmp(name, "csqrtl") == 0) {
        address.complex_long_double_unary = complex_import_csqrtl;
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

static void assert_complex_object_data(const ObjectFile* object,
                                       const char* symbol_name,
                                       SectionType expected_section,
                                       double expected_real,
                                       double expected_imaginary) {
    ObjSymbol* symbol = objfile_find_symbol((ObjectFile*)object, symbol_name);
    ObjSection* section = object->sections;
    double components[2];
    bool valid;
    assert(symbol != NULL && symbol->section >= 0 &&
           (size_t)symbol->section < (size_t)object->section_count);
    for (int index = 0; index < symbol->section; ++index) {
        assert(section != NULL);
        section = section->next;
    }
    valid = section != NULL && section->type == expected_section &&
            section->data != NULL && section->size >= sizeof(components) &&
            symbol->value <= section->size - sizeof(components);
    if (!valid) {
        fprintf(stderr,
                "complex initializer %s: section=%s type=%d expected=%d "
                "section-size=%llu value=%llu\n",
                symbol_name, section && section->name ? section->name : "?",
                section ? (int)section->type : -1, (int)expected_section,
                (unsigned long long)(section ? section->size : 0u),
                (unsigned long long)symbol->value);
    }
    assert(valid);
    memcpy(components, section->data + symbol->value, sizeof(components));
    assert(components[0] == expected_real &&
           components[1] == expected_imaginary);
}

int main(int argc, char** argv) {
    ObjectFile* object;
    ObjectFile* x86_object = NULL;
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
    ComplexDoubleToReal api_creal;
    ComplexDoubleToReal api_cimag;
    ComplexDoubleToReal api_cabs;
    ComplexDoubleToReal api_carg;
    ComplexDoubleUnary api_conj;
    ComplexDoubleUnary api_csqrt;
    ComplexFloatToReal api_crealf;
    ComplexFloatToReal api_cimagf;
    ComplexFloatToReal api_cabsf;
    ComplexFloatToReal api_cargf;
    ComplexFloatUnary api_conjf;
    ComplexFloatUnary api_csqrtf;
    ComplexLongDoubleToReal api_creall;
    ComplexLongDoubleToReal api_cimagl;
    ComplexLongDoubleToReal api_cabsl;
    ComplexLongDoubleToReal api_cargl;
    ComplexLongDoubleUnary api_conjl;
    ComplexLongDoubleUnary api_csqrtl;
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
    ComplexLocalTest static_initializer;
    ComplexPairUnary long_double_identity;
    ComplexPairBinary long_double_add;
    ComplexPairBinary long_double_multiply;
    ComplexPairBinary long_double_divide;
    RccComplexFloat a = __builtin_complex(2.0f, 3.0f);
    RccComplexFloat b = __builtin_complex(4.0f, -1.0f);
    RccComplexFloat value_float;
    RccComplexDouble x = __builtin_complex(2.0, 3.0);
    RccComplexDouble y = __builtin_complex(4.0, -1.0);
    RccComplexDouble value_double;

    assert(argc == 2 || argc == 3);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    assert_complex_object_data(object, "rcc_complex_static_value",
                               SECT_DATA, 7.0, -8.0);
    assert_complex_object_data(object, "rcc_complex_tls_value",
                               SECT_TLS, 9.0, 10.0);
    if (argc == 3) {
        x86_object = objfile_read(argv[2]);
        assert(x86_object != NULL && x86_object->arch == ARCH_X86);
        assert_complex_object_data(x86_object, "rcc_complex_static_value",
                                   SECT_DATA, 7.0, -8.0);
        assert_complex_object_data(x86_object, "rcc_complex_tls_value",
                                   SECT_TLS, 9.0, 10.0);
    }
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
                             "rcc_complex_static_initializer");
    memcpy(&static_initializer, &address, sizeof(static_initializer));
    address = symbol_address(mapped, object,
                             "rcc_complex_long_double_identity");
    memcpy(&long_double_identity, &address, sizeof(long_double_identity));
    address = symbol_address(mapped, object,
                             "rcc_complex_long_double_add");
    memcpy(&long_double_add, &address, sizeof(long_double_add));
    address = symbol_address(mapped, object,
                             "rcc_complex_long_double_multiply");
    memcpy(&long_double_multiply, &address, sizeof(long_double_multiply));
    address = symbol_address(mapped, object,
                             "rcc_complex_long_double_divide");
    memcpy(&long_double_divide, &address, sizeof(long_double_divide));
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
    address = symbol_address(mapped, object, "rcc_complex_api_creal");
    memcpy(&api_creal, &address, sizeof(api_creal));
    address = symbol_address(mapped, object, "rcc_complex_api_cimag");
    memcpy(&api_cimag, &address, sizeof(api_cimag));
    address = symbol_address(mapped, object, "rcc_complex_api_cabs");
    memcpy(&api_cabs, &address, sizeof(api_cabs));
    address = symbol_address(mapped, object, "rcc_complex_api_carg");
    memcpy(&api_carg, &address, sizeof(api_carg));
    address = symbol_address(mapped, object, "rcc_complex_api_conj");
    memcpy(&api_conj, &address, sizeof(api_conj));
    address = symbol_address(mapped, object, "rcc_complex_api_csqrt");
    memcpy(&api_csqrt, &address, sizeof(api_csqrt));
    address = symbol_address(mapped, object, "rcc_complex_api_crealf");
    memcpy(&api_crealf, &address, sizeof(api_crealf));
    address = symbol_address(mapped, object, "rcc_complex_api_cimagf");
    memcpy(&api_cimagf, &address, sizeof(api_cimagf));
    address = symbol_address(mapped, object, "rcc_complex_api_cabsf");
    memcpy(&api_cabsf, &address, sizeof(api_cabsf));
    address = symbol_address(mapped, object, "rcc_complex_api_cargf");
    memcpy(&api_cargf, &address, sizeof(api_cargf));
    address = symbol_address(mapped, object, "rcc_complex_api_conjf");
    memcpy(&api_conjf, &address, sizeof(api_conjf));
    address = symbol_address(mapped, object, "rcc_complex_api_csqrtf");
    memcpy(&api_csqrtf, &address, sizeof(api_csqrtf));
    address = symbol_address(mapped, object, "rcc_complex_api_creall");
    memcpy(&api_creall, &address, sizeof(api_creall));
    address = symbol_address(mapped, object, "rcc_complex_api_cimagl");
    memcpy(&api_cimagl, &address, sizeof(api_cimagl));
    address = symbol_address(mapped, object, "rcc_complex_api_cabsl");
    memcpy(&api_cabsl, &address, sizeof(api_cabsl));
    address = symbol_address(mapped, object, "rcc_complex_api_cargl");
    memcpy(&api_cargl, &address, sizeof(api_cargl));
    address = symbol_address(mapped, object, "rcc_complex_api_conjl");
    memcpy(&api_conjl, &address, sizeof(api_conjl));
    address = symbol_address(mapped, object, "rcc_complex_api_csqrtl");
    memcpy(&api_csqrtl, &address, sizeof(api_csqrtl));

    value_float = float_identity(a);
    assert(close_float(__real__ value_float, 2.0f) &&
           close_float(__imag__ value_float, 3.0f));
    value_float = float_add(a, b);
    assert(close_float(__real__ value_float, 6.0f) &&
           close_float(__imag__ value_float, 2.0f));
    value_float = float_multiply(a, b);
    assert(close_float(__real__ value_float, 11.0f) &&
           close_float(__imag__ value_float, 10.0f));
    value_float = float_multiply(
        __builtin_complex(HUGE_VALF, HUGE_VALF),
        __builtin_complex(1.0f, 0.0f));
    assert(isinf(__real__ value_float) && isinf(__imag__ value_float));
    value_float = float_divide(a, __builtin_complex(1.0f, 1.0f));
    assert(close_float(__real__ value_float, 2.5f) &&
           close_float(__imag__ value_float, 0.5f));
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    value_float = float_divide(a, __builtin_complex(0.0f, 0.0f));
    assert(isinf(__real__ value_float) && isinf(__imag__ value_float));
    assert((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) != 0);
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    value_float = float_divide(__builtin_complex(0.0f, 0.0f),
                               __builtin_complex(0.0f, 0.0f));
    assert(isnan(__real__ value_float) && isnan(__imag__ value_float));
    assert((fetestexcept(FE_ALL_EXCEPT) & (FE_DIVBYZERO | FE_INVALID)) ==
           (FE_DIVBYZERO | FE_INVALID));
    value_float = float_divide(
        __builtin_complex(1.0f, 2.0f),
        __builtin_complex(HUGE_VALF, HUGE_VALF));
    assert(__real__ value_float == 0.0f && __imag__ value_float == 0.0f);
    value_float = float_divide(
        __builtin_complex(HUGE_VALF, HUGE_VALF),
        __builtin_complex(1.0f, 0.0f));
    assert(isinf(__real__ value_float) && isinf(__imag__ value_float));
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
    assert(static_initializer() == 0);

    {
        ComplexBinary64Pair lhs = {2.0, 3.0};
        ComplexBinary64Pair rhs = {4.0, -1.0};
        ComplexBinary64Pair value = long_double_identity(lhs);
        assert(close_double(value.real, 2.0) &&
               close_double(value.imaginary, 3.0));
        value = long_double_add(lhs, rhs);
        assert(close_double(value.real, 6.0) &&
               close_double(value.imaginary, 2.0));
        value = long_double_multiply(lhs, rhs);
        assert(close_double(value.real, 11.0) &&
               close_double(value.imaginary, 10.0));
        rhs.real = 1.0;
        rhs.imaginary = 1.0;
        value = long_double_divide(lhs, rhs);
        assert(close_double(value.real, 2.5) &&
               close_double(value.imaginary, 0.5));
    }

    value_double = double_identity(x);
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, 3.0));
    value_double = double_add(x, y);
    assert(close_double(__real__ value_double, 6.0) &&
           close_double(__imag__ value_double, 2.0));
    value_double = double_multiply(x, y);
    assert(close_double(__real__ value_double, 11.0) &&
           close_double(__imag__ value_double, 10.0));
    value_double = double_multiply(
        __builtin_complex(HUGE_VAL, HUGE_VAL),
        __builtin_complex(1.0, 0.0));
    assert(isinf(__real__ value_double) && isinf(__imag__ value_double));
    value_double = double_divide(x, __builtin_complex(1.0, 1.0));
    assert(close_double(__real__ value_double, 2.5) &&
           close_double(__imag__ value_double, 0.5));
    assert(close_double(api_creal(x), 2.0));
    assert(close_double(api_cimag(x), 3.0));
    assert(close_double(api_cabs(__builtin_complex(3.0, 4.0)), 5.0));
    assert(close_double(api_carg(__builtin_complex(0.0, 1.0)),
                        1.57079632679489661923));
    value_double = api_conj(x);
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, -3.0));
    value_double = api_csqrt(__builtin_complex(3.0, 4.0));
    assert(close_double(__real__ value_double, 2.0) &&
           close_double(__imag__ value_double, 1.0));
    assert(close_float(api_crealf(a), 2.0f));
    assert(close_float(api_cimagf(a), 3.0f));
    assert(close_float(api_cabsf(__builtin_complex(3.0f, 4.0f)), 5.0f));
    assert(close_float(api_cargf(__builtin_complex(0.0f, 1.0f)),
                       1.57079632679489661923f));
    value_float = api_conjf(a);
    assert(close_float(__real__ value_float, 2.0f) &&
           close_float(__imag__ value_float, -3.0f));
    value_float = api_csqrtf(__builtin_complex(3.0f, 4.0f));
    assert(close_float(__real__ value_float, 2.0f) &&
           close_float(__imag__ value_float, 1.0f));
    {
        ComplexBinary64Pair api_long_input = {3.0, 4.0};
        ComplexBinary64Pair api_long_result;
        assert(close_double(api_creall(api_long_input), 3.0));
        assert(close_double(api_cimagl(api_long_input), 4.0));
        assert(close_double(api_cabsl(api_long_input), 5.0));
        assert(close_double(api_cargl(api_long_input),
                            0.92729521800161223243));
        api_long_input.real = 2.0;
        api_long_input.imaginary = 3.0;
        api_long_result = api_conjl(api_long_input);
        assert(close_double(api_long_result.real, 2.0) &&
               close_double(api_long_result.imaginary, -3.0));
        api_long_input.real = 3.0;
        api_long_input.imaginary = 4.0;
        api_long_result = api_csqrtl(api_long_input);
        assert(close_double(api_long_result.real, 2.0) &&
               close_double(api_long_result.imaginary, 1.0));
    }
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    value_double = double_divide(x, __builtin_complex(0.0, 0.0));
    assert(isinf(__real__ value_double) && isinf(__imag__ value_double));
    assert((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) != 0);
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    value_double = double_divide(__builtin_complex(0.0, 0.0),
                                 __builtin_complex(0.0, 0.0));
    assert(isnan(__real__ value_double) && isnan(__imag__ value_double));
    assert((fetestexcept(FE_ALL_EXCEPT) & (FE_DIVBYZERO | FE_INVALID)) ==
           (FE_DIVBYZERO | FE_INVALID));
    value_double = double_divide(
        __builtin_complex(1.0, 2.0),
        __builtin_complex(HUGE_VAL, HUGE_VAL));
    assert(__real__ value_double == 0.0 && __imag__ value_double == 0.0);
    value_double = double_divide(
        __builtin_complex(HUGE_VAL, HUGE_VAL),
        __builtin_complex(1.0, 0.0));
    assert(isinf(__real__ value_double) && isinf(__imag__ value_double));

    unmap_object(mapped);
    objfile_free(object);
    objfile_free(x86_object);
    puts("RCC complex arithmetic and SysV ABI execution passed");
    return 0;
}
