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

/* .rin relocation entry types (must match kernel/make_rin.py) */
#define RIN_RELOC_ABS32 1
#define RIN_RELOC_ABS64 2

/* Relocation entry */
typedef struct Reloc {
    uint32_t offset;        /* Offset in code section */
    uint32_t type;          /* Relocation type */
    const char* symbol;     /* Symbol name (for imports) */
    struct Reloc* next;
} Reloc;

/* String literal entry */
typedef struct StringLit {
    const char* value;
    uint32_t offset;        /* Offset in data section */
    struct StringLit* next;
} StringLit;

/* Module symbol entry (for object files) */
typedef struct ModuleSymbol {
    const char* name;
    uint32_t offset;
    uint32_t size;
    bool is_defined;
    bool is_code;
    bool is_global;
} ModuleSymbol;

/* Module relocation entry (for object files) */
typedef struct ModuleReloc {
    uint32_t offset;
    uint32_t target;
    bool is_relative;
    bool is_64bit;
    const char* symbol_name;
} ModuleReloc;

/* Compiled module */
typedef struct Module {
    CodeSection code;
    DataSection data;
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
void add_reloc(Module* mod, uint32_t offset, uint32_t type);

/* Current code offset */
uint32_t code_offset(Module* mod);

/* x86-64 code generation */
Module* rcc_codegen64(AST* ast);

/* Symbol table functions */
void module_add_symbol(Module* mod, const char* name, uint32_t offset,
                       bool is_defined, bool is_code, bool is_global);
void module_add_relocation(Module* mod, uint32_t offset, uint32_t target,
                          bool is_relative, bool is_64bit,
                          const char* symbol_name);
void codegen_emit_global_data(Module* mod, AST* ast);

/* Object file output */
bool rcc_emit_obj(Module* mod, const char* filename);

#endif /* CODEGEN_H */
