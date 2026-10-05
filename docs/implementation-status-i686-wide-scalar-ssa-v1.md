# i686 wide-scalar typed SSA status

2026-10-06: RinCompiler now keeps each i686 64-bit integer in verified typed
SSA as an explicit low/high pair of 32-bit values. Pair construction is checked
at the lowering boundary. Narrow integer and pointer extension, conversion
back to narrow integers/pointers, 64-bit shift operands, arithmetic, compares,
conditional phi nodes, cdecl argument flattening, and EDX:EAX returns use the
pair path. A scalar 64-bit result is rejected before it can enter the i686
32-bit MIR allocation path.

The aggregate/vector/exception typed-SSA parent item remains incomplete. This
change does not claim full C17/C++20 lowering or target runtime acceptance.
No build or tests were run for this change.
