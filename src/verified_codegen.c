/*
 * RCC - production bridge for the verified typed-SSA x86 backend
 */

#include "rcc.h"
#include "verified_codegen.h"

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
        if (declaration && declaration->kind == DECL_FUNC &&
            declaration->func_body &&
            declaration->storage == STORAGE_STATIC &&
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
        if (item->decl && item->decl->kind == DECL_VAR) {
            return verified_reason(
                RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
                "translation unit contains global data");
        }
    }
    object = objfile_new(translation_unit, arch);
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
