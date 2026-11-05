template<float Value>
struct FixedFloat {};

template<auto Value>
struct DeducedFloat {};

template<float... Values>
struct FixedFloatPack {};

FixedFloat<1.25f> fixed_float;
DeducedFloat<1.25f> deduced_float;
FixedFloatPack<1.25f> fixed_float_pack;
