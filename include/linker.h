/*
 * RLD - RinOS Linker
 * Links multiple .ro object files into .rin executable
 */

#ifndef LINKER_H
#define LINKER_H

#include "rcc.h"
#include "objfile.h"
#include "rin_formats_v3.h"

#define RLD_MAX_DEPENDENCIES 64
#define RLD_MAX_IMPORTS 4096

typedef struct LinkImportSpec {
    const char* symbol;
    const char* dependency;
    uint16_t kind;
} LinkImportSpec;

/* Linker options */
typedef struct {
    char output_file[RCC_MAX_PATH];
    char** input_files;
    int input_count;
    uint16_t arch;
    bool arch_explicit;
    bool verbose;
    bool shared;        /* Create shared library (.rll) */
    uint32_t base_addr; /* Base load address */
    const char* entry;  /* Entry point symbol */
    const char* dependencies[RLD_MAX_DEPENDENCIES];
    int dependency_count;
    LinkImportSpec imports[RLD_MAX_IMPORTS];
    int import_count;
} LinkerOpts;

/* Global linker options */
extern LinkerOpts g_linker_opts;

/* Linked output section */
typedef struct LinkedSection {
    const char* name;
    SectionType type;
    uint32_t flags;
    uint8_t* data;
    uint32_t size;
    uint32_t capacity;
    uint32_t align;
    uint32_t vaddr;     /* Virtual address after linking */
    struct LinkedSection* next;
} LinkedSection;

/* Global symbol (after linking) */
typedef struct GlobalSymbol {
    const char* name;
    uint32_t value;     /* Final resolved address */
    uint32_t size;
    SymbolType type;
    SymbolBinding binding;
    int section;        /* Index into linked sections */
    const char* source; /* Source object file */
    bool resolved;
    struct GlobalSymbol* next;
} GlobalSymbol;

/* Pending relocation */
typedef struct PendingReloc {
    uint32_t offset;        /* Offset in linked section */
    const char* symbol;     /* Symbol name */
    RelocType type;
    int32_t addend;
    int section;            /* Which linked section */
    const char* source;     /* Source object file */
    struct PendingReloc* next;
} PendingReloc;

/* Linker state */
typedef struct Linker {
    /* Input object files */
    ObjectFile** objects;
    int object_count;

    /* Linked sections */
    LinkedSection* sections;
    int section_count;

    /* Global symbol table */
    GlobalSymbol* symbols;
    int symbol_count;

    /* Pending relocations */
    PendingReloc* relocs;
    int reloc_count;

    /* Layout info */
    uint32_t base_addr;
    uint32_t entry_addr;
} Linker;

/* Linker functions */
Linker* linker_new(void);
void linker_free(Linker* ld);

/* Add object files */
bool linker_add_object(Linker* ld, const char* filename);
bool linker_add_objects(Linker* ld, char** files, int count);

/* Linking process */
bool linker_merge_sections(Linker* ld);
bool linker_collect_symbols(Linker* ld);
bool linker_resolve_symbols(Linker* ld);
bool linker_apply_relocations(Linker* ld);
bool linker_layout(Linker* ld, uint32_t base_addr);

/* Output */
bool linker_emit_rin(Linker* ld, const char* filename);
bool linker_emit_rll(Linker* ld, const char* filename);

/* High-level link function */
bool rld_link(char** input_files, int count, const char* output);

#endif /* LINKER_H */
