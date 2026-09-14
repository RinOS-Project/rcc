/*
 * RCC - RinOS C Compiler
 * Code Generation Header
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "rcc.h"
#include "ast.h"

/* Code section */
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} CodeSection;

/* Data section */
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} DataSection;

typedef struct {
    size_t size;
    uint32_t align;
} BssSection;

/* .rin relocation entry types (must match kernel/make_rin.py) */
#define RIN_RELOC_ABS32 1
#define RIN_RELOC_ABS64 2
#define RIN_RELOC_TLSOFF32S 5

typedef enum ModuleSymbolSection {
    MODULE_SYMBOL_CODE,
    MODULE_SYMBOL_RODATA,
    MODULE_SYMBOL_DATA,
    MODULE_SYMBOL_BSS,
    MODULE_SYMBOL_TLS,
    MODULE_SYMBOL_INIT_ARRAY,
    MODULE_SYMBOL_FINI_ARRAY,
} ModuleSymbolSection;

/* Relocation entry */
typedef struct Reloc {
    ModuleSymbolSection source_section;
    uint32_t offset;        /* Offset in source section */
    uint32_t type;          /* Relocation type */
    const char* symbol;     /* Symbol name (for imports) */
    struct Reloc* next;
} Reloc;

/* String literal entry */
typedef struct StringLit {
    const char* value;
    uint32_t offset;        /* Offset in read-only data section */
    struct StringLit* next;
} StringLit;

/* Module symbol entry (for object files) */
typedef struct ModuleSymbol {
    const char* name;
    uint32_t offset;
    uint32_t size;
    bool is_defined;
    ModuleSymbolSection section;
    bool is_global;
    bool is_weak;
} ModuleSymbol;

/* Module relocation entry (for object files) */
typedef struct ModuleReloc {
    ModuleSymbolSection source_section;
    uint32_t offset;
    uint32_t target;
    bool is_relative;
    bool is_64bit;
    bool is_tls;
    const char* symbol_name;
} ModuleReloc;

/* A scalar global initializer which cannot be represented in the image's
 * static data payload.  The frontend has already type-checked the expression;
 * code generation emits it into the translation unit's init function. */
typedef struct GlobalInitializer {
    Decl* declaration;
    struct GlobalInitializer* next;
} GlobalInitializer;

/* A validated static-storage cleanup expression emitted into the module's
 * finalizer callback.  The list is kept in reverse declaration order so
 * destruction follows C++ reverse construction order. */
typedef struct GlobalFinalizer {
    Expr* expression;
    struct GlobalFinalizer* next;
} GlobalFinalizer;

/* Compiled module */
typedef struct Module {
    CodeSection code;
    DataSection rodata;
    DataSection init_array;
    DataSection fini_array;
    DataSection data;
    BssSection bss;
    DataSection tls;
    uint32_t tls_align;
    Reloc* relocs;
    StringLit* strings;
    uint32_t entry_point;
    int stack_size;

    /* Symbol table for object files */
    ModuleSymbol* symbols;
    int symbol_count;
    int symbol_capacity;

    /* Relocation table for object files */
    ModuleReloc* relocs_arr;
    int reloc_count;
    int reloc_capacity;

    GlobalInitializer* global_initializers;
    int global_initializer_count;
    GlobalFinalizer* global_finalizers;
    int global_finalizer_count;
} Module;

/* Code generation functions */
Module* codegen_new(void);
void codegen_free(Module* mod);

/* Emit functions */
void emit_byte(Module* mod, uint8_t b);
void emit_word(Module* mod, uint16_t w);
void emit_dword(Module* mod, uint32_t d);
void emit_bytes(Module* mod, const uint8_t* data, size_t len);

/* Data section */
uint32_t emit_string(Module* mod, const char* str);
uint32_t emit_data(Module* mod, const void* data, size_t len);

/* Relocations */
void add_reloc(Module* mod, ModuleSymbolSection source_section,
               uint32_t offset, uint32_t type);

/* Current code offset */
uint32_t code_offset(Module* mod);

/* x86-64 code generation */
Module* rcc_codegen64(AST* ast);

/* Symbol table functions */
void module_add_symbol(Module* mod, const char* name, uint32_t offset,
                       bool is_defined, ModuleSymbolSection section,
                       bool is_global);
void module_mark_symbol_weak(Module* mod, const char* name);
void module_add_relocation(Module* mod, ModuleSymbolSection source_section,
                          uint32_t offset, uint32_t target,
                          bool is_relative, bool is_64bit,
                          const char* symbol_name);
void module_add_tls_relocation(Module* mod,
                               ModuleSymbolSection source_section,
                               uint32_t offset, const char* symbol_name);
bool module_resolve_image_relocation(const Module* mod,
                                     ModuleSymbolSection source_section,
                                     uint32_t offset,
                                     bool is_64bit, uint64_t rodata_rva,
                                     uint64_t data_rva, uint64_t bss_rva,
                                     uint64_t* value);
bool module_resolve_tls_relocation(const Module* mod,
                                   ModuleSymbolSection source_section,
                                   uint32_t offset, uint32_t* value);
void module_ensure_rodata_base_symbol(Module* mod);
void codegen_add_init_array_entry(Module* mod, const char* symbol);
void codegen_add_fini_array_entry(Module* mod, const char* symbol);
void codegen_emit_global_data(Module* mod, AST* ast);
void codegen_emit_cxx_vtables(Module* mod);
int codegen_required_local_bytes(Stmt* statement);
int codegen_assign_compound_storage(Stmt* statement, int initial_bytes,
                                    int stack_alignment);

/* Object file output */
struct ObjectFile;
struct ObjectFile* module_to_objfile(Module* mod, const char* filename);
bool rcc_emit_obj(Module* mod, const char* filename);

#endif /* CODEGEN_H */
