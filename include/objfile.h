/*
 * RinOS Object File Format (.ro)
 * Relocatable object file for linking
 */

#ifndef OBJFILE_H
#define OBJFILE_H

#include "rcc.h"

/* Magic number: "RO2\0" */
#define RO_MAGIC 0x00324F52u

/* Object file version */
#define RO_VERSION 0x00020000u

/* Symbol types */
typedef enum {
    SYM_UNDEF = 0,      /* Undefined (external reference) */
    SYM_LOCAL = 1,      /* Local symbol (not visible outside) */
    SYM_GLOBAL = 2,     /* Global symbol (exported) */
    SYM_WEAK = 3,       /* Weak symbol (can be overridden) */
} SymbolType;

/* Symbol binding */
typedef enum {
    BIND_CODE = 0,      /* Symbol is in code section */
    BIND_DATA = 1,      /* Symbol is in data section */
    BIND_BSS = 2,       /* Symbol is in BSS section */
    BIND_ABS = 3,       /* Absolute value (not relocated) */
} SymbolBinding;

/* Relocation types */
typedef enum {
    RELOC_ABS32 = 0,    /* 32-bit absolute address */
    RELOC_ABS64 = 1,    /* 64-bit absolute address */
    RELOC_REL32 = 2,    /* 32-bit PC-relative */
    RELOC_REL8 = 3,     /* 8-bit PC-relative */
    RELOC_GOT32 = 4,    /* 32-bit GOT offset (for shared libs) */
    RELOC_PLT32 = 5,    /* 32-bit PLT offset (for shared libs) */
} RelocType;

/* Section types */
typedef enum {
    SECT_NULL = 0,      /* Unused */
    SECT_CODE = 1,      /* Executable code (.text) */
    SECT_DATA = 2,      /* Initialized data (.data) */
    SECT_RODATA = 3,    /* Read-only data (.rodata) */
    SECT_BSS = 4,       /* Uninitialized data (.bss) */
    SECT_TLS = 5,       /* Thread-local storage template */
    SECT_UNWIND = 6,    /* DWARF unwind records */
    SECT_INIT_ARRAY = 7,/* Process/library initializers */
    SECT_FINI_ARRAY = 8,/* Process/library finalizers */
} SectionType;

/* ═══════════════════════════════════════
 * File Header (32 bytes)
 * ═══════════════════════════════════════ */
typedef struct {
    uint32_t magic;         /* RO_MAGIC */
    uint32_t version;       /* File format version */
    uint16_t arch;          /* Target architecture (ARCH_X86/ARCH_X64) */
    uint16_t header_size;   /* sizeof(RoHeader) */
    uint32_t flags;         /* File flags */
    uint32_t section_count; /* Number of sections */
    uint32_t symbol_count;  /* Number of symbols */
    uint64_t section_off;   /* Offset to section headers */
    uint64_t symbol_off;    /* Offset to symbol table */
    uint64_t strtab_off;    /* Offset to string table */
    uint64_t strtab_size;   /* Size of string table */
    uint64_t file_size;     /* Complete object size */
} RoHeader;

/* ═══════════════════════════════════════
 * Section Header (32 bytes)
 * ═══════════════════════════════════════ */
typedef struct {
    uint32_t name;          /* Offset in string table */
    uint32_t type;          /* SectionType */
    uint32_t flags;         /* Section flags */
    uint32_t align;         /* Alignment requirement */
    uint64_t offset;        /* Offset in file */
    uint64_t size;          /* Size in file */
    uint64_t memory_size;   /* Runtime size */
    uint64_t reloc_off;     /* Offset to relocation entries */
    uint32_t reloc_count;   /* Number of relocation entries */
    uint32_t reserved0;
    uint64_t reserved1;
} RoSection;

/* Section flags */
#define SECT_FLAG_WRITE     0x01    /* Writable */
#define SECT_FLAG_EXEC      0x02    /* Executable */
#define SECT_FLAG_ALLOC     0x04    /* Occupies memory at runtime */

/* ═══════════════════════════════════════
 * Symbol Entry (20 bytes)
 * ═══════════════════════════════════════ */
typedef struct {
    uint32_t name;          /* Offset in string table */
    uint16_t type;          /* SymbolType */
    uint16_t binding;       /* SymbolBinding */
    uint32_t section;       /* Section index (0 = undefined) */
    uint32_t flags;
    uint64_t value;         /* Symbol value (offset in section) */
    uint64_t size;          /* Size of symbol (for data) */
    uint64_t reserved;
} RoSymbol;

/* ═══════════════════════════════════════
 * Relocation Entry (12 bytes)
 * ═══════════════════════════════════════ */
typedef struct {
    uint64_t offset;        /* Offset in section where to apply */
    uint32_t symbol;        /* Symbol index */
    uint16_t type;          /* RelocType */
    uint16_t flags;
    int64_t addend;         /* Addend value */
    uint64_t reserved;
} RoReloc;

#if defined(__cplusplus)
static_assert(sizeof(RoHeader) == 64, "RoHeader v2 ABI drift");
static_assert(sizeof(RoSection) == 64, "RoSection v2 ABI drift");
static_assert(sizeof(RoSymbol) == 40, "RoSymbol v2 ABI drift");
static_assert(sizeof(RoReloc) == 32, "RoReloc v2 ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RoHeader) == 64, "RoHeader v2 ABI drift");
_Static_assert(sizeof(RoSection) == 64, "RoSection v2 ABI drift");
_Static_assert(sizeof(RoSymbol) == 40, "RoSymbol v2 ABI drift");
_Static_assert(sizeof(RoReloc) == 32, "RoReloc v2 ABI drift");
#endif

/* ═══════════════════════════════════════
 * In-memory Object File
 * ═══════════════════════════════════════ */

/* Symbol table entry (in-memory) */
typedef struct ObjSymbol {
    const char* name;
    uint64_t value;
    uint64_t size;
    SymbolType type;
    SymbolBinding binding;
    int section;            /* -1 for undefined */
    struct ObjSymbol* next;
} ObjSymbol;

/* Relocation entry (in-memory) */
typedef struct ObjReloc {
    uint64_t offset;
    const char* symbol_name;
    int symbol_idx;
    RelocType type;
    int64_t addend;
    int section;            /* Which section this reloc is in */
    struct ObjReloc* next;
} ObjReloc;

/* Section (in-memory) */
typedef struct ObjSection {
    const char* name;
    SectionType type;
    uint32_t flags;
    uint8_t* data;
    uint64_t size;
    uint64_t capacity;
    uint32_t align;
    ObjReloc* relocs;
    struct ObjSection* next;
} ObjSection;

/* Object file (in-memory) */
typedef struct ObjectFile {
    const char* filename;
    uint16_t arch;

    /* Sections */
    ObjSection* sections;
    int section_count;

    /* Symbols */
    ObjSymbol* symbols;
    int symbol_count;

    /* String table */
    char* strtab;
    uint32_t strtab_size;
    uint32_t strtab_cap;
} ObjectFile;

/* ═══════════════════════════════════════
 * Object File Functions
 * ═══════════════════════════════════════ */

/* Create/destroy object file */
ObjectFile* objfile_new(const char* filename, uint16_t arch);
void objfile_free(ObjectFile* obj);

/* Section operations */
ObjSection* objfile_add_section(ObjectFile* obj, const char* name, SectionType type, uint32_t flags);
ObjSection* objfile_get_section(ObjectFile* obj, const char* name);
uint64_t section_add_data(ObjSection* sect, const void* data, uint64_t size);
uint64_t section_add_byte(ObjSection* sect, uint8_t byte);
uint64_t section_add_bytes(ObjSection* sect, const uint8_t* bytes, uint64_t count);
void section_align(ObjSection* sect, uint32_t align);

/* Symbol operations */
ObjSymbol* objfile_add_symbol(ObjectFile* obj, const char* name, SymbolType type,
                              SymbolBinding binding, int section, uint64_t value,
                              uint64_t size);
ObjSymbol* objfile_find_symbol(ObjectFile* obj, const char* name);

/* Relocation operations */
void objfile_add_reloc(ObjectFile* obj, int section, uint64_t offset,
                       const char* symbol, RelocType type, int64_t addend);

/* String table */
uint32_t objfile_add_string(ObjectFile* obj, const char* str);

/* File I/O */
bool objfile_write(ObjectFile* obj, const char* filename);
ObjectFile* objfile_read(const char* filename);
ObjectFile* objfile_read_memory(const void* data, uint64_t size,
                                const char* display_name);

#endif /* OBJFILE_H */
