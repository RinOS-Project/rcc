import rcc.header.unit.graph;

#ifndef RCC_HEADER_UNIT_GRAPH_EXPECTED
#define RCC_HEADER_UNIT_GRAPH_EXPECTED 24
#endif
#ifndef RCC_HEADER_UNIT_GRAPH_LEAF_EXPECTED
#define RCC_HEADER_UNIT_GRAPH_LEAF_EXPECTED 16
#endif

static_assert(rcc_header_unit_graph_value == RCC_HEADER_UNIT_GRAPH_EXPECTED);
static_assert(rcc_header_unit_graph_leaf_value ==
              RCC_HEADER_UNIT_GRAPH_LEAF_EXPECTED);
