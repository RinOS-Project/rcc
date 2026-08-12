/*
 * RCC - MIR phi edge-copy lowering
 */

#include "rcc.h"
#include "mir_phi.h"

#include <stdarg.h>

static bool mir_phi_error(char* error, size_t error_size,
                          const char* format, ...) {
    va_list arguments;
    if (error && error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static bool mir_phi_location_equal(RccMirLocation left,
                                   RccMirLocation right) {
    if (left.kind != right.kind ||
        left.register_class != right.register_class) {
        return false;
    }
    if (left.kind == RCC_MIR_LOCATION_PHYSICAL) {
        return left.physical_register == right.physical_register;
    }
    return left.spill_offset == right.spill_offset &&
        left.spill_size == right.spill_size;
}

static uint16_t mir_phi_type_size(
    RccMirType type, const RccMirRegisterPolicy* policy) {
    if (type.kind == RCC_MIR_TYPE_POINTER) return policy->pointer_size;
    if (type.bit_width <= 8u) return 1u;
    return (uint16_t)(type.bit_width / 8u);
}

static uint16_t mir_phi_type_alignment(
    RccMirType type, const RccMirRegisterPolicy* policy) {
    uint16_t size = mir_phi_type_size(type, policy);
    if (size > policy->stack_alignment) return policy->stack_alignment;
    return size;
}

static bool mir_phi_align(uint32_t value, uint16_t alignment,
                          uint32_t* result) {
    uint32_t mask = (uint32_t)alignment - 1u;
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u ||
        value > UINT32_MAX - mask) {
        return false;
    }
    *result = (value + mask) & ~mask;
    return true;
}

static RccMirBlock** mir_phi_collect_blocks(
    const RccMirFunction* function) {
    RccMirBlock** blocks = rcc_alloc(
        function->block_count * sizeof(*blocks));
    const RccMirBlock* block;
    for (block = function->first_block; block; block = block->next) {
        blocks[block->id] = (RccMirBlock*)block;
    }
    return blocks;
}

static size_t mir_phi_count_block_phis(const RccMirBlock* block) {
    size_t count = 0u;
    const RccMirInstruction* instruction;
    for (instruction = block->first;
         instruction && instruction->opcode == RCC_MIR_PHI;
         instruction = instruction->next) {
        ++count;
    }
    return count;
}

static size_t mir_phi_predecessor_count(
    RccMirBlock** blocks, size_t block_count, RccMirBlockId successor) {
    size_t count = 0u;
    size_t block;
    for (block = 0u; block < block_count; ++block) {
        const RccMirInstruction* terminator = blocks[block]->last;
        size_t target;
        for (target = 0u; target < terminator->target_count; ++target) {
            if (terminator->targets[target] == successor) {
                ++count;
                break;
            }
        }
    }
    return count;
}

static RccMirEdgeCopies* mir_phi_find_edge(
    RccMirPhiPlan* plan, RccMirBlockId predecessor,
    RccMirBlockId successor) {
    size_t index;
    for (index = 0u; index < plan->edge_count; ++index) {
        if (plan->edges[index].predecessor == predecessor &&
            plan->edges[index].successor == successor) {
            return &plan->edges[index];
        }
    }
    return NULL;
}

static bool mir_phi_append_edge(RccMirPhiPlan* plan,
                                RccMirBlockId predecessor,
                                RccMirBlockId successor,
                                bool requires_edge_block,
                                RccMirEdgeCopies** edge_out) {
    RccMirEdgeCopies* resized;
    if (plan->edge_count == SIZE_MAX / sizeof(*plan->edges)) return false;
    resized = rcc_realloc(plan->edges,
                          (plan->edge_count + 1u) * sizeof(*plan->edges));
    plan->edges = resized;
    memset(&plan->edges[plan->edge_count], 0,
           sizeof(plan->edges[plan->edge_count]));
    plan->edges[plan->edge_count].predecessor = predecessor;
    plan->edges[plan->edge_count].successor = successor;
    plan->edges[plan->edge_count].requires_edge_block =
        requires_edge_block;
    *edge_out = &plan->edges[plan->edge_count++];
    return true;
}

static bool mir_phi_append_copy(RccMirEdgeCopies* edge,
                                RccMirVReg source,
                                RccMirVReg destination,
                                RccMirType type) {
    RccMirParallelCopy* resized;
    if (edge->copy_count == SIZE_MAX / sizeof(*edge->copies)) return false;
    resized = rcc_realloc(edge->copies,
                          (edge->copy_count + 1u) * sizeof(*edge->copies));
    edge->copies = resized;
    edge->copies[edge->copy_count].source = source;
    edge->copies[edge->copy_count].destination = destination;
    edge->copies[edge->copy_count].type = type;
    ++edge->copy_count;
    return true;
}

static bool mir_phi_append_move(RccMirEdgeCopies* edge,
                                RccMirLocation source,
                                RccMirLocation destination,
                                RccMirType type, bool cycle_break) {
    RccMirScheduledMove* resized;
    if (edge->move_count == SIZE_MAX / sizeof(*edge->moves)) return false;
    resized = rcc_realloc(edge->moves,
                          (edge->move_count + 1u) * sizeof(*edge->moves));
    edge->moves = resized;
    edge->moves[edge->move_count].source = source;
    edge->moves[edge->move_count].destination = destination;
    edge->moves[edge->move_count].type = type;
    edge->moves[edge->move_count].cycle_break = cycle_break;
    ++edge->move_count;
    return true;
}

static bool mir_phi_reserve_cycle_temporary(
    RccMirPhiPlan* plan, const RccMirRegisterPolicy* policy,
    uint16_t maximum_size, uint16_t maximum_alignment,
    char* error, size_t error_size) {
    uint32_t offset;
    if (plan->has_cycle_temporary) return true;
    if (!mir_phi_align(plan->frame_size, maximum_alignment, &offset) ||
        offset > UINT32_MAX - maximum_size) {
        return mir_phi_error(error, error_size,
                             "MIR phi temporary exceeds stack offsets");
    }
    plan->has_cycle_temporary = true;
    plan->cycle_temporary_offset = offset;
    plan->cycle_temporary_size = maximum_size;
    plan->cycle_temporary_alignment = maximum_alignment;
    plan->frame_size = offset + maximum_size;
    if (!mir_phi_align(plan->frame_size, policy->stack_alignment,
                       &plan->frame_size)) {
        return mir_phi_error(error, error_size,
                             "MIR phi frame alignment overflow");
    }
    return true;
}

static RccMirLocation mir_phi_temporary_location(
    const RccMirPhiPlan* plan, RccMirRegisterClass register_class) {
    RccMirLocation location;
    memset(&location, 0, sizeof(location));
    location.kind = RCC_MIR_LOCATION_SPILL;
    location.register_class = register_class;
    location.physical_register = UINT16_MAX;
    location.spill_offset = plan->cycle_temporary_offset;
    location.spill_size = plan->cycle_temporary_size;
    location.spill_alignment = plan->cycle_temporary_alignment;
    return location;
}

static bool mir_phi_schedule_edge(
    RccMirEdgeCopies* edge, const RccMirFunction* function,
    const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, RccMirPhiPlan* plan,
    uint16_t maximum_size, uint16_t maximum_alignment,
    char* error, size_t error_size) {
    RccMirLocation* sources;
    RccMirLocation* destinations;
    bool* pending;
    size_t remaining = 0u;
    size_t index;
    if (edge->copy_count == 0u) return true;
    sources = rcc_alloc(edge->copy_count * sizeof(*sources));
    destinations = rcc_alloc(edge->copy_count * sizeof(*destinations));
    pending = rcc_alloc(edge->copy_count * sizeof(*pending));
    for (index = 0u; index < edge->copy_count; ++index) {
        sources[index] = allocation->locations[edge->copies[index].source];
        destinations[index] =
            allocation->locations[edge->copies[index].destination];
        if (!mir_phi_location_equal(sources[index], destinations[index])) {
            pending[index] = true;
            ++remaining;
        }
    }
    while (remaining != 0u) {
        bool progress = false;
        for (index = 0u; index < edge->copy_count; ++index) {
            size_t other;
            bool destination_is_source = false;
            if (!pending[index]) continue;
            for (other = 0u; other < edge->copy_count; ++other) {
                if (pending[other] && other != index &&
                    mir_phi_location_equal(destinations[index],
                                           sources[other])) {
                    destination_is_source = true;
                    break;
                }
            }
            if (destination_is_source) continue;
            if (!mir_phi_append_move(edge, sources[index],
                                     destinations[index],
                                     edge->copies[index].type, false)) {
                rcc_free(sources);
                rcc_free(destinations);
                rcc_free(pending);
                return mir_phi_error(error, error_size,
                                     "MIR phi move table is too large");
            }
            pending[index] = false;
            --remaining;
            progress = true;
        }
        if (!progress) {
            RccMirLocation temporary;
            size_t other;
            for (index = 0u; index < edge->copy_count; ++index) {
                if (pending[index]) break;
            }
            if (index == edge->copy_count ||
                !mir_phi_reserve_cycle_temporary(
                    plan, policy, maximum_size, maximum_alignment,
                    error, error_size)) {
                rcc_free(sources);
                rcc_free(destinations);
                rcc_free(pending);
                return false;
            }
            temporary = mir_phi_temporary_location(
                plan, destinations[index].register_class);
            if (!mir_phi_append_move(edge, destinations[index], temporary,
                                     edge->copies[index].type, true)) {
                rcc_free(sources);
                rcc_free(destinations);
                rcc_free(pending);
                return mir_phi_error(error, error_size,
                                     "MIR phi move table is too large");
            }
            for (other = 0u; other < edge->copy_count; ++other) {
                if (pending[other] &&
                    mir_phi_location_equal(sources[other],
                                           destinations[index])) {
                    sources[other] = temporary;
                }
            }
        }
    }
    rcc_free(sources);
    rcc_free(destinations);
    rcc_free(pending);
    (void)function;
    return true;
}

static bool mir_phi_build_parallel_copies(
    const RccMirFunction* function, RccMirBlock** blocks,
    RccMirPhiPlan* plan, char* error, size_t error_size) {
    size_t successor;
    for (successor = 0u; successor < function->block_count; ++successor) {
        const RccMirBlock* block = blocks[successor];
        const RccMirInstruction* phi;
        size_t phi_count = mir_phi_count_block_phis(block);
        size_t predecessor_count;
        if (phi_count == 0u) continue;
        predecessor_count = mir_phi_predecessor_count(
            blocks, function->block_count, (RccMirBlockId)successor);
        for (phi = block->first;
             phi && phi->opcode == RCC_MIR_PHI; phi = phi->next) {
            size_t incoming;
            for (incoming = 0u; incoming < phi->operand_count; ++incoming) {
                RccMirBlockId predecessor = phi->targets[incoming];
                RccMirEdgeCopies* edge = mir_phi_find_edge(
                    plan, predecessor, (RccMirBlockId)successor);
                if (!edge) {
                    bool critical =
                        blocks[predecessor]->last->target_count > 1u &&
                        predecessor_count > 1u;
                    if (!mir_phi_append_edge(
                            plan, predecessor, (RccMirBlockId)successor,
                            critical, &edge)) {
                        return mir_phi_error(
                            error, error_size,
                            "MIR phi edge table is too large");
                    }
                }
                if (!mir_phi_append_copy(edge, phi->operands[incoming],
                                         phi->definition, phi->type)) {
                    return mir_phi_error(error, error_size,
                                         "MIR phi copy table is too large");
                }
            }
        }
    }
    return true;
}

static size_t mir_phi_location_index(
    RccMirLocation* locations, size_t count, RccMirLocation location) {
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (mir_phi_location_equal(locations[index], location)) return index;
    }
    return SIZE_MAX;
}

static bool mir_phi_verify_schedule(
    const RccMirEdgeCopies* edge, const RccMirAllocation* allocation,
    char* error, size_t error_size) {
    RccMirLocation* locations;
    size_t* initial_tokens;
    size_t* current_tokens;
    size_t location_count = 0u;
    size_t capacity;
    size_t index;
    if (edge->copy_count > SIZE_MAX / 2u ||
        edge->move_count > SIZE_MAX / 2u ||
        edge->copy_count * 2u >
            SIZE_MAX - edge->move_count * 2u) {
        return mir_phi_error(error, error_size,
                             "MIR phi verification table is too large");
    }
    capacity = edge->copy_count * 2u + edge->move_count * 2u;
    if (capacity == 0u) return true;
    if (capacity > SIZE_MAX / sizeof(*locations)) {
        return mir_phi_error(error, error_size,
                             "MIR phi verification table is too large");
    }
    locations = rcc_alloc(capacity * sizeof(*locations));
    for (index = 0u; index < edge->copy_count; ++index) {
        RccMirLocation source = allocation->locations[
            edge->copies[index].source];
        RccMirLocation destination = allocation->locations[
            edge->copies[index].destination];
        if (mir_phi_location_index(locations, location_count, source) ==
            SIZE_MAX) {
            locations[location_count++] = source;
        }
        if (mir_phi_location_index(locations, location_count, destination) ==
            SIZE_MAX) {
            locations[location_count++] = destination;
        }
    }
    for (index = 0u; index < edge->move_count; ++index) {
        if (mir_phi_location_index(locations, location_count,
                                  edge->moves[index].source) == SIZE_MAX) {
            locations[location_count++] = edge->moves[index].source;
        }
        if (mir_phi_location_index(locations, location_count,
                                  edge->moves[index].destination) ==
            SIZE_MAX) {
            locations[location_count++] = edge->moves[index].destination;
        }
    }
    initial_tokens = rcc_alloc(location_count * sizeof(*initial_tokens));
    current_tokens = rcc_alloc(location_count * sizeof(*current_tokens));
    for (index = 0u; index < location_count; ++index) {
        initial_tokens[index] = index;
        current_tokens[index] = index;
    }
    for (index = 0u; index < edge->move_count; ++index) {
        size_t source = mir_phi_location_index(
            locations, location_count, edge->moves[index].source);
        size_t destination = mir_phi_location_index(
            locations, location_count, edge->moves[index].destination);
        if (source == SIZE_MAX || destination == SIZE_MAX) {
            rcc_free(locations);
            rcc_free(initial_tokens);
            rcc_free(current_tokens);
            return mir_phi_error(error, error_size,
                                 "MIR phi move names an invalid location");
        }
        current_tokens[destination] = current_tokens[source];
    }
    for (index = 0u; index < edge->copy_count; ++index) {
        size_t source = mir_phi_location_index(
            locations, location_count,
            allocation->locations[edge->copies[index].source]);
        size_t destination = mir_phi_location_index(
            locations, location_count,
            allocation->locations[edge->copies[index].destination]);
        if (current_tokens[destination] != initial_tokens[source]) {
            rcc_free(locations);
            rcc_free(initial_tokens);
            rcc_free(current_tokens);
            return mir_phi_error(
                error, error_size,
                "scheduled MIR phi moves change parallel-copy semantics");
        }
    }
    rcc_free(locations);
    rcc_free(initial_tokens);
    rcc_free(current_tokens);
    return true;
}

void rcc_mir_phi_plan_release(RccMirPhiPlan* plan) {
    size_t index;
    if (!plan) return;
    for (index = 0u; index < plan->edge_count; ++index) {
        rcc_free(plan->edges[index].copies);
        rcc_free(plan->edges[index].moves);
    }
    rcc_free(plan->edges);
    memset(plan, 0, sizeof(*plan));
}

bool rcc_mir_verify_phi_plan(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, const RccMirPhiPlan* plan,
    char* error, size_t error_size) {
    RccMirBlock** blocks;
    size_t edge_index;
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !policy || !allocation || !plan ||
        !rcc_mir_verify_function(function, error, error_size) ||
        !rcc_mir_verify_allocation(function, policy, allocation,
                                   error, error_size)) {
        return false;
    }
    if (plan->frame_size < allocation->spill_area_size ||
        plan->frame_size % policy->stack_alignment != 0u ||
        (plan->has_cycle_temporary &&
         (plan->cycle_temporary_alignment == 0u ||
          plan->cycle_temporary_offset %
              plan->cycle_temporary_alignment != 0u ||
          plan->cycle_temporary_offset > plan->frame_size ||
          plan->cycle_temporary_size > plan->frame_size -
              plan->cycle_temporary_offset))) {
        return mir_phi_error(error, error_size,
                             "MIR phi frame layout is invalid");
    }
    blocks = mir_phi_collect_blocks(function);
    for (edge_index = 0u; edge_index < plan->edge_count; ++edge_index) {
        const RccMirEdgeCopies* edge = &plan->edges[edge_index];
        size_t phi_count;
        size_t copy_index;
        bool edge_exists = false;
        if (edge->predecessor >= function->block_count ||
            edge->successor >= function->block_count) {
            rcc_free(blocks);
            return mir_phi_error(error, error_size,
                                 "MIR phi edge is out of range");
        }
        for (size_t target = 0u;
             target < blocks[edge->predecessor]->last->target_count;
             ++target) {
            if (blocks[edge->predecessor]->last->targets[target] ==
                edge->successor) {
                edge_exists = true;
                break;
            }
        }
        phi_count = mir_phi_count_block_phis(blocks[edge->successor]);
        if (!edge_exists || edge->copy_count != phi_count) {
            rcc_free(blocks);
            return mir_phi_error(error, error_size,
                                 "MIR phi edge copies do not match CFG");
        }
        {
            bool expected_critical =
                blocks[edge->predecessor]->last->target_count > 1u &&
                mir_phi_predecessor_count(
                    blocks, function->block_count, edge->successor) > 1u;
            if (edge->requires_edge_block != expected_critical) {
                rcc_free(blocks);
                return mir_phi_error(
                    error, error_size,
                    "MIR phi critical-edge classification is invalid");
            }
        }
        for (copy_index = 0u; copy_index < edge->copy_count;
             ++copy_index) {
            const RccMirInstruction* phi =
                blocks[edge->successor]->first;
            size_t incoming;
            for (size_t seek = 0u; seek < copy_index; ++seek) {
                phi = phi->next;
            }
            if (!phi || phi->opcode != RCC_MIR_PHI ||
                edge->copies[copy_index].destination != phi->definition ||
                !rcc_mir_type_equal(edge->copies[copy_index].type,
                                    phi->type)) {
                rcc_free(blocks);
                return mir_phi_error(error, error_size,
                                     "MIR phi copy is inconsistent");
            }
            for (incoming = 0u; incoming < phi->target_count; ++incoming) {
                if (phi->targets[incoming] == edge->predecessor) break;
            }
            if (incoming == phi->target_count ||
                edge->copies[copy_index].source != phi->operands[incoming]) {
                rcc_free(blocks);
                return mir_phi_error(error, error_size,
                                     "MIR phi source is inconsistent");
            }
        }
        for (size_t earlier = 0u; earlier < edge_index; ++earlier) {
            if (plan->edges[earlier].predecessor == edge->predecessor &&
                plan->edges[earlier].successor == edge->successor) {
                rcc_free(blocks);
                return mir_phi_error(error, error_size,
                                     "MIR phi plan repeats an edge");
            }
        }
        if (!mir_phi_verify_schedule(edge, allocation,
                                     error, error_size)) {
            rcc_free(blocks);
            return false;
        }
    }
    for (size_t successor = 0u; successor < function->block_count;
         ++successor) {
        if (mir_phi_count_block_phis(blocks[successor]) != 0u) {
            for (size_t predecessor = 0u;
                 predecessor < function->block_count; ++predecessor) {
                const RccMirInstruction* terminator =
                    blocks[predecessor]->last;
                bool expected = false;
                for (size_t target = 0u;
                     target < terminator->target_count; ++target) {
                    if (terminator->targets[target] == successor) {
                        expected = true;
                        break;
                    }
                }
                if (expected && !mir_phi_find_edge(
                        (RccMirPhiPlan*)plan, (RccMirBlockId)predecessor,
                        (RccMirBlockId)successor)) {
                    rcc_free(blocks);
                    return mir_phi_error(error, error_size,
                                         "MIR phi plan omits an edge");
                }
            }
        }
    }
    rcc_free(blocks);
    return true;
}

bool rcc_mir_build_phi_plan(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, RccMirPhiPlan* plan,
    char* error, size_t error_size) {
    RccMirBlock** blocks;
    uint16_t maximum_size = 1u;
    uint16_t maximum_alignment = 1u;
    size_t edge;
    if (plan) memset(plan, 0, sizeof(*plan));
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !policy || !allocation || !plan ||
        !rcc_mir_verify_function(function, error, error_size) ||
        !rcc_mir_verify_allocation(function, policy, allocation,
                                   error, error_size)) {
        return false;
    }
    plan->frame_size = allocation->spill_area_size;
    blocks = mir_phi_collect_blocks(function);
    if (!mir_phi_build_parallel_copies(function, blocks, plan,
                                       error, error_size)) {
        rcc_free(blocks);
        rcc_mir_phi_plan_release(plan);
        return false;
    }
    rcc_free(blocks);
    for (edge = 0u; edge < plan->edge_count; ++edge) {
        size_t copy;
        for (copy = 0u; copy < plan->edges[edge].copy_count; ++copy) {
            uint16_t size = mir_phi_type_size(
                plan->edges[edge].copies[copy].type, policy);
            uint16_t alignment = mir_phi_type_alignment(
                plan->edges[edge].copies[copy].type, policy);
            if (size > maximum_size) maximum_size = size;
            if (alignment > maximum_alignment) {
                maximum_alignment = alignment;
            }
        }
    }
    for (edge = 0u; edge < plan->edge_count; ++edge) {
        if (!mir_phi_schedule_edge(
                &plan->edges[edge], function, policy, allocation, plan,
                maximum_size, maximum_alignment, error, error_size)) {
            rcc_mir_phi_plan_release(plan);
            return false;
        }
    }
    if (!plan->has_cycle_temporary &&
        !mir_phi_align(plan->frame_size, policy->stack_alignment,
                       &plan->frame_size)) {
        rcc_mir_phi_plan_release(plan);
        return mir_phi_error(error, error_size,
                             "MIR phi frame alignment overflow");
    }
    if (!rcc_mir_verify_phi_plan(function, policy, allocation, plan,
                                 error, error_size)) {
        rcc_mir_phi_plan_release(plan);
        return false;
    }
    return true;
}
