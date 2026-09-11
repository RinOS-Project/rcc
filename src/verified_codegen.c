/*
 * RCC - production bridge for the verified typed-SSA x86 backend
 */

#include "rcc.h"
#include "verified_codegen.h"

#include "codegen.h"
#include "ir_lower.h"
#include "objfile.h"
#include "x86_object.h"
#include "x86_pipeline.h"

#include <stdarg.h>

static RccVerifiedObjectStatus verified_reason(
    RccVerifiedObjectStatus status, char* reason, size_t reason_size,
    const char* format, ...) {
    if (reason && reason_size != 0u) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(reason, reason_size, format, arguments);
        va_end(arguments);
    }
    return status;
}

static const Decl* verified_static_definition(
    const AST* ast, const char* name) {
    const DeclList* item;
    for (item = ast->decls; item; item = item->next) {
        const Decl* declaration = item->decl;
        bool is_definition = declaration &&
            ((declaration->kind == DECL_FUNC && declaration->func_body) ||
             (declaration->kind == DECL_VAR &&
              declaration->var_is_global &&
              !(declaration->storage == STORAGE_EXTERN &&
                !declaration->var_init)));
        if (is_definition && declaration->storage == STORAGE_STATIC &&
            strcmp(decl_link_name(declaration), name) == 0) {
            return declaration;
        }
    }
    return NULL;
}

static char* verified_scoped_symbol(
    const char* translation_unit, const char* name) {
    size_t unit_length = strlen(translation_unit);
    size_t name_length = strlen(name);
    char* scoped = rcc_alloc(unit_length + name_length + 3u);
    memcpy(scoped, translation_unit, unit_length);
    scoped[unit_length] = ':';
    scoped[unit_length + 1u] = ':';
    memcpy(scoped + unit_length + 2u, name, name_length + 1u);
    return scoped;
}

static int verified_section_index(
    const ObjectFile* object, const ObjSection* target) {
    int index = 0;
    for (const ObjSection* section = object->sections; section;
         section = section->next, ++index) {
        if (section == target) return index;
    }
    return -1;
}

static char* verified_constant_symbol(
    const char* translation_unit, const char* function_name,
    const char* constant_name) {
    size_t unit_length = strlen(translation_unit);
    size_t function_length = strlen(function_name);
    size_t constant_length = strlen(constant_name);
    size_t prefix;
    size_t total;
    char* scoped;
    if (function_length > SIZE_MAX - 5u ||
        unit_length > SIZE_MAX - function_length - 5u) return NULL;
    prefix = unit_length + function_length + 5u;
    if (constant_length > SIZE_MAX - prefix) return NULL;
    total = prefix + constant_length;
    scoped = rcc_alloc(total);
    snprintf(scoped, total, "%s::%s::%s", translation_unit,
             function_name, constant_name);
    return scoped;
}

static bool verified_add_constants(
    ObjectFile* object, const RccIrModule* module,
    const char* translation_unit, const char* function_name,
    RccX86EncodedFunction* encoded,
    char* error, size_t error_size) {
    ObjSection* rodata = objfile_get_section(object, ".rodata");
    int section_index;
    if (rodata &&
        (rodata->type != SECT_RODATA ||
         (rodata->flags & SECT_FLAG_ALLOC) == 0u ||
         (rodata->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) != 0u)) {
        if (error && error_size != 0u) {
            snprintf(error, error_size,
                     "verified .rodata contract is invalid");
        }
        return false;
    }
    for (const RccIrConstant* constant = module->first_constant;
         constant; constant = constant->next) {
        size_t references = 0u;
        char* symbol;
        uint64_t offset;
        for (size_t index = 0u; index < encoded->relocation_count; ++index) {
            if (strcmp(encoded->relocations[index].symbol,
                       constant->name) == 0) ++references;
        }
        if (references == 0u) continue;
        if (!rodata) {
            rodata = objfile_add_section(
                object, ".rodata", SECT_RODATA, SECT_FLAG_ALLOC);
        }
        symbol = verified_constant_symbol(
            translation_unit, function_name, constant->name);
        if (!symbol || objfile_find_symbol(object, symbol)) {
            rcc_free(symbol);
            if (error && error_size != 0u) {
                snprintf(error, error_size,
                         "verified constant symbol is invalid or duplicate");
            }
            return false;
        }
        section_align(rodata, constant->alignment);
        offset = section_add_data(rodata, constant->data, constant->size);
        section_index = verified_section_index(object, rodata);
        if (section_index < 0) {
            rcc_free(symbol);
            if (error && error_size != 0u) {
                snprintf(error, error_size,
                         "verified .rodata section is detached");
            }
            return false;
        }
        objfile_add_symbol(
            object, symbol, SYM_LOCAL, BIND_DATA, section_index,
            offset, constant->size);
        for (size_t index = 0u; index < encoded->relocation_count; ++index) {
            RccX86CodeRelocation* relocation = &encoded->relocations[index];
            if (strcmp(relocation->symbol, constant->name) == 0) {
                rcc_free(relocation->symbol);
                relocation->symbol = rcc_strdup(symbol);
            }
        }
        rcc_free(symbol);
    }
    return true;
}

static void verified_scope_static_relocations(
    const AST* ast, const char* translation_unit,
    RccX86EncodedFunction* encoded) {
    size_t index;
    for (index = 0u; index < encoded->relocation_count; ++index) {
        RccX86CodeRelocation* relocation = &encoded->relocations[index];
        if (verified_static_definition(ast, relocation->symbol)) {
            char* scoped = verified_scoped_symbol(
                translation_unit, relocation->symbol);
            rcc_free(relocation->symbol);
            relocation->symbol = scoped;
        }
    }
}

RccVerifiedObjectStatus rcc_emit_verified_object(
    const AST* ast, const char* translation_unit,
    const char* output_path, size_t* function_count,
    char* reason, size_t reason_size) {
    const DeclList* item;
    Module* data_module = NULL;
    ObjectFile* object = NULL;
    size_t emitted = 0u;
    uint16_t arch = g_opts.target_arch == ARCH_X64 ? ARCH_X64 : ARCH_X86;
    RccX86Target target = g_opts.target_arch == ARCH_X64
        ? RCC_X86_TARGET_X86_64 : RCC_X86_TARGET_I686;
    if (function_count) *function_count = 0u;
    if (reason && reason_size != 0u) reason[0] = '\0';
    if (!ast || !translation_unit || !translation_unit[0] ||
        !output_path || !output_path[0]) {
        return verified_reason(
            RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
            "invalid verified object request");
    }
    for (item = ast->decls; item; item = item->next) {
        if (item->decl && item->decl->kind == DECL_VAR &&
            item->decl->var_is_global &&
            item->decl->var_is_thread_local) {
            return verified_reason(
                RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
                "translation unit contains thread-local data");
        }
    }
    data_module = codegen_new();
    codegen_emit_global_data(data_module, (AST*)ast);
    object = module_to_objfile(data_module, translation_unit);
    codegen_free(data_module);
    if (!object || object->arch != arch) {
        objfile_free(object);
        return verified_reason(
            RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
            "failed to emit verified global data");
    }
    for (item = ast->decls; item; item = item->next) {
        const Decl* declaration = item->decl;
        RccIrModule* module = NULL;
        RccIrLowerStatus lower_status;
        RccX86EncodedFunction encoded;
        SymbolType symbol_type;
        const char* object_name;
        char* scoped_name = NULL;
        char pipeline_error[256];
        memset(&encoded, 0, sizeof(encoded));
        if (!declaration || declaration->kind != DECL_FUNC ||
            !declaration->func_body) continue;
        lower_status = rcc_ir_lower_function(
            declaration, &module, pipeline_error,
            sizeof(pipeline_error));
        if (lower_status != RCC_IR_LOWER_OK) {
            objfile_free(object);
            if (lower_status == RCC_IR_LOWER_UNSUPPORTED) {
                return verified_reason(
                    RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
                    "function '%s' is outside the typed SSA subset",
                    declaration->name);
            }
            return verified_reason(
                RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
                "function '%s' failed typed SSA validation: %s",
                declaration->name, pipeline_error);
        }
        if (!module || !module->first_function ||
            module->first_function != module->last_function ||
            !rcc_x86_encode_ir_function(
                module->first_function, target, &encoded,
                pipeline_error, sizeof(pipeline_error))) {
            rcc_ir_module_destroy(module);
            objfile_free(object);
            rcc_x86_encoded_function_release(&encoded);
            return verified_reason(
                RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
                "function '%s' is not encoded yet: %s",
                declaration->name, pipeline_error);
        }
        if (!verified_add_constants(
                object, module, translation_unit,
                decl_link_name(declaration), &encoded,
                pipeline_error, sizeof(pipeline_error))) {
            rcc_x86_encoded_function_release(&encoded);
            rcc_ir_module_destroy(module);
            objfile_free(object);
            return verified_reason(
                RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
                "function '%s' constant emission failed: %s",
                declaration->name, pipeline_error);
        }
        verified_scope_static_relocations(ast, translation_unit, &encoded);
        object_name = decl_link_name(declaration);
        if (declaration->storage == STORAGE_STATIC) {
            scoped_name = verified_scoped_symbol(
                translation_unit, object_name);
            object_name = scoped_name;
            symbol_type = SYM_LOCAL;
        } else if (declaration->func_is_inline &&
                   declaration->func_has_cxx_linkage) {
            symbol_type = SYM_WEAK;
        } else {
            symbol_type = SYM_GLOBAL;
        }
        if (!rcc_x86_object_add_function(
                object, object_name, symbol_type, &encoded,
                pipeline_error, sizeof(pipeline_error))) {
            rcc_free(scoped_name);
            rcc_x86_encoded_function_release(&encoded);
            rcc_ir_module_destroy(module);
            objfile_free(object);
            return verified_reason(
                RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
                "function '%s' object emission failed: %s",
                declaration->name, pipeline_error);
        }
        rcc_free(scoped_name);
        rcc_x86_encoded_function_release(&encoded);
        rcc_ir_module_destroy(module);
        ++emitted;
    }
    if (emitted == 0u) {
        objfile_free(object);
        return verified_reason(
            RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
            "translation unit has no supported function definitions");
    }
    if (!objfile_write(object, output_path)) {
        objfile_free(object);
        return verified_reason(
            RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
            "failed to write verified .ro v2 object");
    }
    objfile_free(object);
    if (function_count) *function_count = emitted;
    return RCC_VERIFIED_OBJECT_EMITTED;
}
