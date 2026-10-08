/*
 * RCC - production bridge for the verified typed-SSA x86 backend
 */

#include "rcc.h"
#include "verified_codegen.h"

#include "codegen.h"
#include "ir_lower.h"
#include "objfile.h"
#include "x86_encode.h"
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

static bool verified_record_debug_frame_epilogues(
    Module* module, const ObjSymbol* function_symbol,
    const RccX86EncodedFunction* encoded, char* error, size_t error_size) {
    if (!module || !function_symbol || !encoded ||
        function_symbol->binding != BIND_CODE ||
        function_symbol->section < 0 ||
        function_symbol->value > UINT32_MAX) {
        if (error && error_size != 0u) {
            snprintf(error, error_size,
                     "verified function epilogue base is invalid");
        }
        return false;
    }
    for (size_t index = 0u; index < encoded->epilogue_count; ++index) {
        const RccX86CodeEpilogue* epilogue = &encoded->epilogues[index];
        uint64_t return_pc = function_symbol->value + epilogue->return_pc;
        uint64_t resume_pc = function_symbol->value + epilogue->resume_pc;
        if (return_pc > UINT32_MAX || resume_pc > UINT32_MAX ||
            resume_pc <= return_pc) {
            if (error && error_size != 0u) {
                snprintf(error, error_size,
                         "verified function epilogue range exceeds 32 bits");
            }
            return false;
        }
        module_add_debug_frame_epilogue(
            module, (uint32_t)return_pc, (uint32_t)resume_pc);
    }
    return true;
}

static bool verified_dwarf_register(
    RccX86Target target, RccX86HardwareGpr hardware_register,
    uint8_t* dwarf_register) {
    if (!dwarf_register) return false;
    if (target == RCC_X86_TARGET_I686) {
        switch (hardware_register) {
            case RCC_X86_GPR_AX: *dwarf_register = 0u; return true;
            case RCC_X86_GPR_CX: *dwarf_register = 1u; return true;
            case RCC_X86_GPR_DX: *dwarf_register = 2u; return true;
            case RCC_X86_GPR_BX: *dwarf_register = 3u; return true;
            case RCC_X86_GPR_SP: *dwarf_register = 4u; return true;
            case RCC_X86_GPR_BP: *dwarf_register = 5u; return true;
            case RCC_X86_GPR_SI: *dwarf_register = 6u; return true;
            case RCC_X86_GPR_DI: *dwarf_register = 7u; return true;
            default: return false;
        }
    }
    if (target == RCC_X86_TARGET_X86_64) {
        switch (hardware_register) {
            case RCC_X86_GPR_AX: *dwarf_register = 0u; return true;
            case RCC_X86_GPR_DX: *dwarf_register = 1u; return true;
            case RCC_X86_GPR_CX: *dwarf_register = 2u; return true;
            case RCC_X86_GPR_BX: *dwarf_register = 3u; return true;
            case RCC_X86_GPR_SI: *dwarf_register = 4u; return true;
            case RCC_X86_GPR_DI: *dwarf_register = 5u; return true;
            case RCC_X86_GPR_BP: *dwarf_register = 6u; return true;
            case RCC_X86_GPR_SP: *dwarf_register = 7u; return true;
            case RCC_X86_GPR_R8: *dwarf_register = 8u; return true;
            case RCC_X86_GPR_R9: *dwarf_register = 9u; return true;
            case RCC_X86_GPR_R10: *dwarf_register = 10u; return true;
            case RCC_X86_GPR_R11: *dwarf_register = 11u; return true;
            case RCC_X86_GPR_R12: *dwarf_register = 12u; return true;
            case RCC_X86_GPR_R13: *dwarf_register = 13u; return true;
            case RCC_X86_GPR_R14: *dwarf_register = 14u; return true;
            case RCC_X86_GPR_R15: *dwarf_register = 15u; return true;
            default: return false;
        }
    }
    return false;
}

static bool verified_record_debug_frame_saves(
    Module* module, const ObjSymbol* function_symbol,
    const RccX86EncodedFunction* encoded, char* error, size_t error_size) {
    if (!module || !function_symbol || !encoded ||
        function_symbol->binding != BIND_CODE ||
        function_symbol->section < 0 ||
        function_symbol->value > UINT32_MAX) {
        if (error && error_size != 0u) {
            snprintf(error, error_size,
                     "verified function callee-save base is invalid");
        }
        return false;
    }
    for (size_t index = 0u; index < encoded->callee_save_count; ++index) {
        const RccX86CodeCalleeSave* source = &encoded->callee_saves[index];
        uint8_t dwarf_register;
        uint64_t save_pc = function_symbol->value + source->save_pc;
        if (!verified_dwarf_register(
                encoded->target, source->gpr, &dwarf_register) ||
            save_pc > UINT32_MAX || save_pc <= function_symbol->value) {
            if (error && error_size != 0u) {
                snprintf(error, error_size,
                         "verified function callee-save record is invalid");
            }
            return false;
        }
        module_add_debug_frame_save(
            module, (uint32_t)save_pc, dwarf_register,
            source->frame_offset);
    }
    return true;
}

static void verified_append_statement_debug_range(
    Stmt* statement, uint32_t start, uint32_t end) {
    StmtDebugRange** insertion;
    if (!statement || end <= start) return;
    insertion = &statement->debug_code_ranges;
    while (*insertion && (*insertion)->end < start) {
        insertion = &(*insertion)->next;
    }
    if (!*insertion || (*insertion)->start > end) {
        StmtDebugRange* added = rcc_alloc(sizeof(*added));
        added->start = start;
        added->end = end;
        added->next = *insertion;
        *insertion = added;
    } else {
        StmtDebugRange* merged = *insertion;
        if (start < merged->start) merged->start = start;
        if (end > merged->end) merged->end = end;
        while (merged->next && merged->next->start <= merged->end) {
            StmtDebugRange* next = merged->next;
            if (next->end > merged->end) merged->end = next->end;
            merged->next = next->next;
            rcc_free(next);
        }
    }
    if (statement->debug_code_end <= statement->debug_code_start) {
        statement->debug_code_start = start;
        statement->debug_code_end = end;
    } else {
        if (start < statement->debug_code_start) {
            statement->debug_code_start = start;
        }
        if (end > statement->debug_code_end) {
            statement->debug_code_end = end;
        }
    }
}

static void verified_append_descendant_debug_ranges(
    Stmt* source, Stmt* target) {
    if (!source || !target) return;
    for (const StmtDebugRange* range = source->debug_code_ranges;
         range; range = range->next) {
        verified_append_statement_debug_range(
            target, range->start, range->end);
    }
    switch (source->kind) {
        case STMT_BLOCK:
            for (StmtList* item = source->block_stmts; item;
                 item = item->next) {
                verified_append_descendant_debug_ranges(item->stmt, target);
            }
            break;
        case STMT_IF:
            verified_append_descendant_debug_ranges(source->if_then, target);
            verified_append_descendant_debug_ranges(source->if_else, target);
            break;
        case STMT_WHILE:
        case STMT_DO:
            verified_append_descendant_debug_ranges(
                source->while_body, target);
            break;
        case STMT_FOR:
            verified_append_descendant_debug_ranges(source->for_init, target);
            verified_append_descendant_debug_ranges(source->for_body, target);
            break;
        case STMT_SWITCH:
            verified_append_descendant_debug_ranges(
                source->switch_body, target);
            break;
        case STMT_CASE:
            verified_append_descendant_debug_ranges(
                source->case_stmt, target);
            break;
        case STMT_DEFAULT:
            verified_append_descendant_debug_ranges(
                source->default_stmt, target);
            break;
        case STMT_LABEL:
            verified_append_descendant_debug_ranges(
                source->label_stmt, target);
            break;
        case STMT_TRY:
            verified_append_descendant_debug_ranges(
                source->try_body, target);
            for (CxxCatch* handler = source->try_catches; handler;
                 handler = handler->next) {
                verified_append_descendant_debug_ranges(handler->body, target);
            }
            break;
        default:
            break;
    }
}

static void verified_propagate_block_debug_ranges(Stmt* statement) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                verified_propagate_block_debug_ranges(item->stmt);
            }
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                verified_append_descendant_debug_ranges(
                    item->stmt, statement);
            }
            break;
        case STMT_IF:
            verified_propagate_block_debug_ranges(statement->if_then);
            verified_propagate_block_debug_ranges(statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            verified_propagate_block_debug_ranges(statement->while_body);
            break;
        case STMT_FOR:
            verified_propagate_block_debug_ranges(statement->for_init);
            verified_propagate_block_debug_ranges(statement->for_body);
            break;
        case STMT_SWITCH:
            verified_propagate_block_debug_ranges(statement->switch_body);
            break;
        case STMT_CASE:
            verified_propagate_block_debug_ranges(statement->case_stmt);
            break;
        case STMT_DEFAULT:
            verified_propagate_block_debug_ranges(statement->default_stmt);
            break;
        case STMT_LABEL:
            verified_propagate_block_debug_ranges(statement->label_stmt);
            break;
        case STMT_TRY:
            verified_propagate_block_debug_ranges(statement->try_body);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                verified_propagate_block_debug_ranges(handler->body);
            }
            break;
        default:
            break;
    }
}

static void verified_clear_statement_debug_ranges(Stmt* statement) {
    StmtDebugRange* range;
    if (!statement) return;
    statement->debug_code_start = 0u;
    statement->debug_code_end = 0u;
    statement->debug_line_offset = 0u;
    statement->debug_line_valid = false;
    range = statement->debug_code_ranges;
    while (range) {
        StmtDebugRange* next = range->next;
        rcc_free(range);
        range = next;
    }
    statement->debug_code_ranges = NULL;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                verified_clear_statement_debug_ranges(item->stmt);
            }
            break;
        case STMT_IF:
            verified_clear_statement_debug_ranges(statement->if_then);
            verified_clear_statement_debug_ranges(statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            verified_clear_statement_debug_ranges(statement->while_body);
            break;
        case STMT_FOR:
            verified_clear_statement_debug_ranges(statement->for_init);
            verified_clear_statement_debug_ranges(statement->for_body);
            break;
        case STMT_SWITCH:
            verified_clear_statement_debug_ranges(statement->switch_body);
            break;
        case STMT_CASE:
            verified_clear_statement_debug_ranges(statement->case_stmt);
            break;
        case STMT_DEFAULT:
            verified_clear_statement_debug_ranges(statement->default_stmt);
            break;
        case STMT_LABEL:
            verified_clear_statement_debug_ranges(statement->label_stmt);
            break;
        case STMT_TRY:
            verified_clear_statement_debug_ranges(statement->try_body);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                verified_clear_statement_debug_ranges(handler->body);
            }
            break;
        default:
            break;
    }
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
        if (!statement || range->offset > UINT32_MAX -
                (uint32_t)object_symbol->value ||
            range->size > UINT32_MAX -
                ((uint32_t)object_symbol->value + range->offset)) {
            continue;
        }
        start = object_symbol->value + range->offset;
        end = start + range->size;
        if (end <= start || end > UINT32_MAX) continue;
        verified_append_statement_debug_range(
            statement, (uint32_t)start, (uint32_t)end);
    }
}

static void verified_emit_debug_sections(
    ObjectFile* object, Module* data_module, const AST* ast,
    const char* translation_unit,
    const DebugVariableLocation* variable_locations,
    size_t variable_location_count) {
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
    debug_module->debug_verified_backend = true;
    debug_module->debug_variable_locations = variable_locations;
    debug_module->debug_variable_location_count = variable_location_count;
    if (text->size != 0u) emit_bytes(debug_module, text->data, text->size);
    for (size_t index = 0u;
         index < data_module->debug_frame_epilogue_count; ++index) {
        const ModuleDebugFrameEpilogue* epilogue =
            &data_module->debug_frame_epilogues[index];
        module_add_debug_frame_epilogue(
            debug_module, epilogue->return_pc, epilogue->resume_pc);
    }
    for (size_t index = 0u;
         index < data_module->debug_frame_save_count; ++index) {
        const ModuleDebugFrameSave* save =
            &data_module->debug_frame_saves[index];
        module_add_debug_frame_save(
            debug_module, save->save_pc, save->dwarf_register,
            save->frame_offset);
    }
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

static void verified_collect_debug_variable_locations(
    DebugVariableLocation** locations, size_t* count, size_t* capacity,
    const RccX86EncodedFunction* encoded) {
    if (!locations || !count || !capacity || !encoded) return;
    for (size_t index = 0u; index < encoded->local_location_count; ++index) {
        const RccX86CodeLocalLocation* source =
            &encoded->local_locations[index];
        size_t next_capacity;
        if (!source->declaration) continue;
        if (*count == *capacity) {
            next_capacity = *capacity == 0u ? 16u : *capacity * 2u;
            if (next_capacity < *capacity ||
                next_capacity > SIZE_MAX / sizeof(**locations)) {
                rcc_fatal("verified DWARF local-location table is too large");
                return;
            }
            *locations = rcc_realloc(
                *locations, next_capacity * sizeof(**locations));
            *capacity = next_capacity;
        }
        (*locations)[*count].declaration =
            (const Decl*)source->declaration;
        (*locations)[*count].frame_offset = source->frame_offset;
        (*locations)[*count].alignment = source->alignment;
        ++*count;
    }
}

RccVerifiedObjectStatus rcc_emit_verified_object(
    const AST* ast, const char* translation_unit,
    const char* output_path, size_t* function_count,
    char* reason, size_t reason_size) {
    const DeclList* item;
    Module* data_module = NULL;
    ObjectFile* object = NULL;
    DebugVariableLocation* debug_variable_locations = NULL;
    size_t debug_variable_location_count = 0u;
    size_t debug_variable_location_capacity = 0u;
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
    data_module = codegen_new();
    codegen_emit_global_data(data_module, (AST*)ast);
    verified_emit_typeinfo_ast(data_module, ast);
    codegen_emit_cxx_vtables(data_module);
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
        if (g_opts.debug_info) {
            verified_clear_statement_debug_ranges(declaration->func_body);
        }
        lower_status = rcc_ir_lower_function(
            declaration, &module, pipeline_error,
            sizeof(pipeline_error));
        if (lower_status != RCC_IR_LOWER_OK) {
            rcc_free(debug_variable_locations);
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
            rcc_free(debug_variable_locations);
            rcc_ir_module_destroy(module);
            objfile_free(object);
            rcc_x86_encoded_function_release(&encoded);
            return verified_reason(
                RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
                "function '%s' is not encoded yet: %s",
                declaration->name, pipeline_error);
        }
        verified_collect_debug_variable_locations(
            &debug_variable_locations,
            &debug_variable_location_count,
            &debug_variable_location_capacity, &encoded);
        if (!verified_add_constants(
                object, module, translation_unit,
                decl_link_name(declaration), &encoded,
                pipeline_error, sizeof(pipeline_error))) {
            rcc_free(debug_variable_locations);
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
            rcc_free(debug_variable_locations);
            rcc_free(scoped_name);
            rcc_x86_encoded_function_release(&encoded);
            rcc_ir_module_destroy(module);
            objfile_free(object);
            return verified_reason(
                RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
                "function '%s' object emission failed: %s",
                declaration->name, pipeline_error);
        }
        if (g_opts.debug_info &&
            (!verified_record_debug_frame_epilogues(
                 data_module, objfile_find_symbol(object, object_name),
                 &encoded, pipeline_error, sizeof(pipeline_error)) ||
             !verified_record_debug_frame_saves(
                 data_module, objfile_find_symbol(object, object_name),
                 &encoded, pipeline_error, sizeof(pipeline_error)))) {
            rcc_free(debug_variable_locations);
            rcc_free(scoped_name);
            rcc_x86_encoded_function_release(&encoded);
            rcc_ir_module_destroy(module);
            objfile_free(object);
            return verified_reason(
                RCC_VERIFIED_OBJECT_INVALID, reason, reason_size,
                "function '%s' debug epilogue recording failed: %s",
                declaration->name, pipeline_error);
        }
        verified_apply_statement_debug_ranges(object, object_name, &encoded);
        verified_propagate_block_debug_ranges(declaration->func_body);
        rcc_free(scoped_name);
        rcc_x86_encoded_function_release(&encoded);
        rcc_ir_module_destroy(module);
        ++emitted;
    }
    if (emitted == 0u) {
        rcc_free(debug_variable_locations);
        codegen_free(data_module);
        objfile_free(object);
        return verified_reason(
            RCC_VERIFIED_OBJECT_FALLBACK, reason, reason_size,
            "translation unit has no supported function definitions");
    }
    verified_emit_debug_sections(
        object, data_module, ast, translation_unit,
        debug_variable_locations, debug_variable_location_count);
    rcc_free(debug_variable_locations);
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
