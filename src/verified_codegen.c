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

static void verified_emit_typeinfo_expr(Module* module, const Expr* expression);

static void verified_emit_typeinfo_expr_list(
    Module* module, const ExprList* list) {
    for (; list; list = list->next) {
        verified_emit_typeinfo_expr(module, list->expr);
    }
}

static void verified_emit_typeinfo_stmt(Module* module, const Stmt* statement) {
    const StmtList* item;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_EXPR:
            verified_emit_typeinfo_expr(module, statement->expr);
            break;
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                verified_emit_typeinfo_stmt(module, item->stmt);
            }
            break;
        case STMT_IF:
            verified_emit_typeinfo_expr(module, statement->if_cond);
            verified_emit_typeinfo_stmt(module, statement->if_then);
            verified_emit_typeinfo_stmt(module, statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            verified_emit_typeinfo_expr(module, statement->while_cond);
            verified_emit_typeinfo_stmt(module, statement->while_body);
            break;
        case STMT_FOR:
            verified_emit_typeinfo_stmt(module, statement->for_init);
            verified_emit_typeinfo_expr(module, statement->for_cond);
            verified_emit_typeinfo_expr(module, statement->for_inc);
            verified_emit_typeinfo_stmt(module, statement->for_body);
            break;
        case STMT_SWITCH:
            verified_emit_typeinfo_expr(module, statement->switch_expr);
            verified_emit_typeinfo_stmt(module, statement->switch_body);
            break;
        case STMT_CASE:
            verified_emit_typeinfo_expr(module, statement->case_val);
            verified_emit_typeinfo_stmt(module, statement->case_stmt);
            break;
        case STMT_DEFAULT:
            verified_emit_typeinfo_stmt(module, statement->default_stmt);
            break;
        case STMT_RETURN:
        case STMT_THROW:
            verified_emit_typeinfo_expr(module, statement->return_val);
            break;
        case STMT_LABEL:
            verified_emit_typeinfo_stmt(module, statement->label_stmt);
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR) {
                verified_emit_typeinfo_expr(
                    module, statement->decl->var_init);
            }
            break;
        case STMT_TRY:
            verified_emit_typeinfo_stmt(module, statement->try_body);
            for (const CxxCatch* handler = statement->try_catches;
                 handler; handler = handler->next) {
                verified_emit_typeinfo_stmt(module, handler->body);
            }
            break;
        default:
            break;
    }
}

static void verified_emit_typeinfo_expr(Module* module, const Expr* expression) {
    const GenericAssociation* association;
    const CxxCompoundRequirement* requirement;
    if (!module || !expression) return;
    if (expression->kind == EXPR_CXX_TYPEID &&
        !expression->cxx_typeid_dynamic &&
        expression->cxx_typeid_symbol &&
        expression->cxx_typeid_symbol[0]) {
        codegen_emit_cxx_typeinfo_symbol(
            module, expression->cxx_typeid_symbol);
    }
    verified_emit_typeinfo_expr(module, expression->cxx_fold_init);
    verified_emit_typeinfo_expr(module, expression->cxx_fold_pattern);
    verified_emit_typeinfo_expr(
        module, expression->cxx_pack_expansion_pattern);
    verified_emit_typeinfo_expr_list(module, expression->cxx_lambda_captures);
    if (expression->cxx_move_assignment) {
        verified_emit_typeinfo_expr(
            module, expression->cxx_move_assignment->source);
        verified_emit_typeinfo_expr(
            module, expression->cxx_move_assignment->cleanup);
        verified_emit_typeinfo_expr(
            module, expression->cxx_move_assignment->release);
    }
    if (expression->cxx_close_call) {
        verified_emit_typeinfo_expr(
            module, expression->cxx_close_call->object);
        verified_emit_typeinfo_expr(
            module, expression->cxx_close_call->handle);
        verified_emit_typeinfo_expr(
            module, expression->cxx_close_call->cleanup);
    }
    switch (expression->kind) {
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            verified_emit_typeinfo_expr(module, expression->unary_operand);
            break;
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        case EXPR_SPACESHIP:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_ASSIGN:
        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
        case EXPR_COMMA:
            verified_emit_typeinfo_expr(module, expression->binary_lhs);
            verified_emit_typeinfo_expr(module, expression->binary_rhs);
            break;
        case EXPR_COND:
            verified_emit_typeinfo_expr(module, expression->cond_test);
            verified_emit_typeinfo_expr(module, expression->cond_then);
            verified_emit_typeinfo_expr(module, expression->cond_else);
            break;
        case EXPR_CALL:
            verified_emit_typeinfo_expr(module, expression->call_func);
            verified_emit_typeinfo_expr_list(module, expression->call_args);
            verified_emit_typeinfo_expr(
                module, expression->call_virtual_object);
            verified_emit_typeinfo_expr(module, expression->call_new_count);
            verified_emit_typeinfo_expr_list(module, expression->call_new_args);
            break;
        case EXPR_INDEX:
            verified_emit_typeinfo_expr(module, expression->index_base);
            verified_emit_typeinfo_expr(module, expression->index_expr);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            verified_emit_typeinfo_expr(module, expression->member_base);
            break;
        case EXPR_CAST:
            verified_emit_typeinfo_expr(module, expression->cast_expr);
            break;
        case EXPR_CXX_TYPEID:
            verified_emit_typeinfo_expr(
                module, expression->cxx_typeid_operand);
            break;
        case EXPR_COMPOUND:
            verified_emit_typeinfo_expr_list(
                module, expression->compound_init);
            break;
        case EXPR_GENERIC:
            verified_emit_typeinfo_expr(module, expression->generic_control);
            for (association = expression->generic_associations;
                 association; association = association->next) {
                verified_emit_typeinfo_expr(module, association->expr);
            }
            break;
        case EXPR_CXX_REQUIRES:
            verified_emit_typeinfo_expr_list(
                module, expression->cxx_requires_items);
            verified_emit_typeinfo_expr_list(
                module, expression->cxx_requires_nested);
            for (requirement = expression->cxx_requires_compound;
                 requirement; requirement = requirement->next) {
                verified_emit_typeinfo_expr(module, requirement->expr);
            }
            break;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            verified_emit_typeinfo_expr(module, expression->va_list_operand);
            verified_emit_typeinfo_expr(module, expression->va_second_operand);
            break;
        default:
            break;
    }
}

static void verified_emit_typeinfo_ast(Module* module, const AST* ast) {
    for (const DeclList* item = ast ? ast->decls : NULL; item;
         item = item->next) {
        const Decl* declaration = item->decl;
        if (!declaration) continue;
        if (declaration->kind == DECL_FUNC) {
            verified_emit_typeinfo_stmt(module, declaration->func_body);
        } else if (declaration->kind == DECL_VAR) {
            verified_emit_typeinfo_expr(module, declaration->var_init);
        } else if (declaration->kind == DECL_STATIC_ASSERT) {
            verified_emit_typeinfo_expr(
                module, declaration->static_assert_expr);
        }
    }
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

static ModuleSymbol* verified_debug_symbol(Module* module, const char* name) {
    if (!module || !name) return NULL;
    for (int index = 0; index < module->symbol_count; ++index) {
        if (strcmp(module->symbols[index].name, name) == 0) {
            return &module->symbols[index];
        }
    }
    return NULL;
}

static void verified_copy_debug_symbol(Module* destination,
                                       const ModuleSymbol* source) {
    ModuleSymbol* copy;
    if (!destination || !source || !source->name) return;
    module_add_symbol(destination, source->name, source->offset,
                     source->is_defined, source->section, source->is_global);
    copy = verified_debug_symbol(destination, source->name);
    if (!copy) {
        rcc_fatal("verified DWARF symbol was not retained");
        return;
    }
    copy->size = source->size;
    copy->is_weak = source->is_weak;
    copy->source_file = source->source_file;
    copy->source_line = source->source_line;
    copy->source_column = source->source_column;
}

static void verified_add_function_debug_symbol(
    Module* debug_module, ObjectFile* object, const AST* ast,
    const char* translation_unit, const Decl* declaration) {
    const char* link_name;
    const char* object_name;
    char* scoped_name = NULL;
    ObjSymbol* object_symbol;
    ModuleSymbol* debug_symbol;
    if (!debug_module || !object || !ast || !translation_unit ||
        !declaration || declaration->kind != DECL_FUNC ||
        !declaration->func_body) return;
    link_name = decl_link_name(declaration);
    if (!link_name || !link_name[0]) return;
    object_name = link_name;
    if (declaration->storage == STORAGE_STATIC) {
        scoped_name = verified_scoped_symbol(translation_unit, link_name);
        object_name = scoped_name;
    }
    object_symbol = objfile_find_symbol(object, object_name);
    if (!object_symbol || object_symbol->binding != BIND_CODE ||
        object_symbol->section < 0 ||
        object_symbol->type == SYM_UNDEF ||
        object_symbol->value > UINT32_MAX ||
        object_symbol->size > UINT32_MAX) {
        rcc_free(scoped_name);
        return;
    }
    module_add_symbol(
        debug_module, link_name, (uint32_t)object_symbol->value, true,
        MODULE_SYMBOL_CODE, declaration->storage != STORAGE_STATIC);
    debug_symbol = verified_debug_symbol(debug_module, link_name);
    if (!debug_symbol) {
        rcc_fatal("verified DWARF function symbol was not retained");
        rcc_free(scoped_name);
        return;
    }
    debug_symbol->size = (uint32_t)object_symbol->size;
    module_set_symbol_source(debug_module, link_name, declaration->loc);
    /* The verified encoder does not run the classic statement codegen pass,
     * so its AST ranges are otherwise left at zero.  The function symbol is
     * the exact text range produced by the verified encoder; expose that
     * range as the function body's coarse lexical scope.  Nested statement
     * ranges remain intentionally unset until MIR source locations exist. */
    if (object_symbol->value <= UINT32_MAX &&
        object_symbol->size <= UINT32_MAX - object_symbol->value) {
        declaration->func_body->debug_code_start =
            (uint32_t)object_symbol->value;
        declaration->func_body->debug_code_end =
            (uint32_t)(object_symbol->value + object_symbol->size);
    }
    rcc_free(scoped_name);
}

static void verified_apply_statement_debug_ranges(
    ObjectFile* object, const char* object_name,
    const RccX86EncodedFunction* encoded) {
    ObjSymbol* object_symbol;
    if (!object || !object_name || !encoded) return;
    object_symbol = objfile_find_symbol(object, object_name);
    if (!object_symbol || object_symbol->value > UINT32_MAX ||
        object_symbol->size > UINT32_MAX - object_symbol->value) return;
    for (size_t index = 0u; index < encoded->source_range_count; ++index) {
        const RccX86CodeSourceRange* range = &encoded->source_ranges[index];
        Stmt* statement = (Stmt*)range->source_statement;
        uint64_t start;
        uint64_t end;
        StmtDebugRange* debug_range;
        if (!statement || range->offset > UINT32_MAX -
                (uint32_t)object_symbol->value ||
            range->size > UINT32_MAX -
                ((uint32_t)object_symbol->value + range->offset)) {
            continue;
        }
        start = object_symbol->value + range->offset;
        end = start + range->size;
        if (end <= start || end > UINT32_MAX) continue;
        debug_range = statement->debug_code_ranges;
        if (!debug_range) {
            debug_range = rcc_alloc(sizeof(*debug_range));
            debug_range->start = (uint32_t)start;
            debug_range->end = (uint32_t)end;
            statement->debug_code_ranges = debug_range;
        } else {
            while (debug_range->next) debug_range = debug_range->next;
            if (start <= debug_range->end) {
                if (end > debug_range->end) {
                    debug_range->end = (uint32_t)end;
                }
            } else {
                StmtDebugRange* next = rcc_alloc(sizeof(*next));
                next->start = (uint32_t)start;
                next->end = (uint32_t)end;
                debug_range->next = next;
            }
        }
        if (statement->debug_code_end <= statement->debug_code_start) {
            statement->debug_code_start = (uint32_t)start;
            statement->debug_code_end = (uint32_t)end;
        } else {
            if (start < statement->debug_code_start) {
                statement->debug_code_start = (uint32_t)start;
            }
            if (end > statement->debug_code_end) {
                statement->debug_code_end = (uint32_t)end;
            }
        }
    }
}

static void verified_emit_debug_sections(
    ObjectFile* object, Module* data_module, const AST* ast,
    const char* translation_unit) {
    ObjSection* text;
    Module* debug_module;
    if (!g_opts.debug_info || !object || !data_module || !ast ||
        !translation_unit || !translation_unit[0]) return;
    text = objfile_get_section(object, ".text");
    if (!text || text->type != SECT_CODE ||
        text->size > SIZE_MAX) {
        rcc_fatal("verified DWARF object has no valid text section");
        return;
    }
    debug_module = codegen_new();
    debug_module->debug_ast = (AST*)ast;
    debug_module->debug_statement_ranges = true;
    if (text->size != 0u) emit_bytes(debug_module, text->data, text->size);
    for (int index = 0; index < data_module->symbol_count; ++index) {
        verified_copy_debug_symbol(debug_module, &data_module->symbols[index]);
    }
    for (const DeclList* item = ast->decls; item; item = item->next) {
        verified_add_function_debug_symbol(
            debug_module, object, ast, translation_unit, item->decl);
    }
    module_emit_debug_sections(object, debug_module, translation_unit);
    codegen_free(debug_module);
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
    verified_emit_typeinfo_ast(data_module, ast);
    /* The verified functions are appended after the data module has already
     * been converted to an object.  Defer all debug emission until those
     * functions are present so one DWARF unit covers data and code without
     * duplicate named sections. */
    {
        bool debug_info = g_opts.debug_info;
        g_opts.debug_info = false;
        object = module_to_objfile(data_module, translation_unit);
        g_opts.debug_info = debug_info;
    }
    if (!object || object->arch != arch) {
        codegen_free(data_module);
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
        } else if (declaration->is_weak ||
                   (declaration->func_is_inline &&
                    declaration->func_has_cxx_linkage)) {
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
        verified_apply_statement_debug_ranges(object, object_name, &encoded);
        rcc_free(scoped_name);
        rcc_x86_encoded_function_release(&encoded);
        rcc_ir_module_destroy(module);
        ++emitted;
    }
    if (emitted == 0u) {
        codegen_free(data_module);
        objfile_free(object);
        return verified_reason(
            RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
            "translation unit has no supported function definitions");
    }
    verified_emit_debug_sections(object, data_module, ast, translation_unit);
    codegen_free(data_module);
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
