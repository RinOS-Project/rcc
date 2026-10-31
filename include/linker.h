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
#define RLD_MAX_EXPORTS 4096

typedef struct LinkImportSpec {
    const char* symbol;
    const char* dependency;
    uint16_t kind;
} LinkImportSpec;

/* An explicit shared-library export.  version is NULL for the unversioned
 * ABI name; otherwise the exact string is stored in RinExportV3.reserved. */
typedef struct LinkExportSpec {
    const char* symbol;
    const char* version;
} LinkExportSpec;

/* Linker options */
typedef struct {
    char output_file[RCC_MAX_PATH];
    bool output_explicit;
    char** input_files;
    int input_count;
    uint16_t arch;
    bool arch_explicit;
    bool verbose;
    bool shared;        /* Create shared library (.rll) */
    bool shared_explicit;
    uint64_t base_addr; /* Base load address */
    const char* entry;  /* Entry point symbol */
    bool entry_explicit;
    char manifest_entry[RCC_MAX_IDENT];
    const char* manifest_path;
    const char* dependencies[RLD_MAX_DEPENDENCIES];
    int dependency_count;
    LinkImportSpec imports[RLD_MAX_IMPORTS];
    int import_count;
    LinkExportSpec exports[RLD_MAX_EXPORTS];
    int export_count;
} LinkerOpts;

/* Global linker options */
extern LinkerOpts g_linker_opts;

/* Linked output section */
typedef struct LinkedSection {
    const char* name;
    SectionType type;
    uint32_t flags;
    uint8_t* data;
    uint64_t size;        /* Bytes stored in the output image */
    uint64_t memory_size; /* Bytes occupied after zero-fill */
    uint64_t capacity;
    uint32_t align;
    uint64_t vaddr;     /* Virtual address after linking */
    struct LinkedSection* next;
} LinkedSection;

/* Global symbol (after linking) */
typedef struct GlobalSymbol {
    const char* name;
    uint64_t value;     /* Final resolved address */
    uint64_t size;
    SymbolType type;
    SymbolBinding binding;
    int section;        /* Index into linked sections */
    const char* source; /* Source object file */
    bool resolved;
    struct GlobalSymbol* next;
    struct GlobalSymbol* hash_next;
} GlobalSymbol;

/* Input symbols are indexed while objects are appended. Archive extraction
 * uses these counts before the final GlobalSymbol table has been collected. */
typedef struct LinkerInputSymbol {
    const char* name;
    size_t definition_count;
    size_t undefined_count;
} LinkerInputSymbol;

/* Pending relocation */
typedef struct PendingReloc {
    uint64_t offset;        /* Offset in linked section */
    const char* symbol;     /* Symbol name */
    RelocType type;
    int64_t addend;
    int section;            /* Which linked section */
    const char* source;     /* Source object file */
    struct PendingReloc* next;
} PendingReloc;

/* Linker state */
typedef struct Linker {
    /* Input object files */
    ObjectFile** objects;
    int object_count;
    size_t object_capacity;
    /* First-object-wins index for input COMDAT group signatures. */
    const char** comdat_keys;
    size_t comdat_key_count;
    size_t comdat_key_capacity;

    /* Open-addressed index of accepted input definitions and references. */
    LinkerInputSymbol* input_symbol_slots;
    size_t input_symbol_count;
    size_t input_symbol_capacity;

    /* Linked sections */
    LinkedSection* sections;
    int section_count;

    /* Global symbol table */
    GlobalSymbol* symbols;
    GlobalSymbol* symbol_tail;
    GlobalSymbol** symbol_buckets;
    size_t symbol_bucket_count;
    int symbol_count;

    /* Pending relocations */
    PendingReloc* relocs;
    PendingReloc* reloc_tail;
    int reloc_count;

    /* Layout info */
    uint64_t base_addr;
    uint64_t entry_addr;
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
bool linker_layout(Linker* ld, uint64_t base_addr);

/* Output */
bool linker_emit_rin(Linker* ld, const char* filename);
bool linker_emit_rll(Linker* ld, const char* filename);

/* High-level link function */
bool rld_link(char** input_files, int count, const char* output);

#endif /* LINKER_H */
