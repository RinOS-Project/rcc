namespace namespace_component_000 {
namespace namespace_component_001 {
namespace namespace_component_002 {
namespace namespace_component_003 {
namespace namespace_component_004 {
namespace namespace_component_005 {
namespace namespace_component_006 {
namespace namespace_component_007 {
namespace namespace_component_008 {
namespace namespace_component_009 {
namespace namespace_component_010 {
namespace namespace_component_011 {
namespace namespace_component_012 {
namespace namespace_component_013 {
namespace namespace_component_014 {
namespace namespace_component_015 {
namespace namespace_component_016 {
namespace namespace_component_017 {
namespace namespace_component_018 {
namespace namespace_component_019 {
namespace namespace_component_020 {
namespace namespace_component_021 {
namespace namespace_component_022 {
namespace namespace_component_023 {
namespace namespace_component_024 {
namespace namespace_component_025 {
namespace namespace_component_026 {
namespace namespace_component_027 {
namespace namespace_component_028 {
namespace namespace_component_029 {
namespace namespace_component_030 {
namespace namespace_component_031 {
namespace namespace_component_032 {
namespace namespace_component_033 {
struct DeepOwner {
    int value;
    static int marker() { return 17; }
};
template<class T>
struct DeepTemplate {};
template<template<class> class Template>
int template_template_probe() {
    return sizeof(Template<int>);
}
inline int run_template_template_probe() {
    return template_template_probe<DeepTemplate>();
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}

namespace using_chain_00 { int value = 29; }
namespace using_chain_01 { using namespace using_chain_00; }
namespace using_chain_02 { using namespace using_chain_01; }
namespace using_chain_03 { using namespace using_chain_02; }
namespace using_chain_04 { using namespace using_chain_03; }
namespace using_chain_05 { using namespace using_chain_04; }
namespace using_chain_06 { using namespace using_chain_05; }
namespace using_chain_07 { using namespace using_chain_06; }
namespace using_chain_08 { using namespace using_chain_07; }
namespace using_chain_09 { using namespace using_chain_08; }
namespace using_chain_10 { using namespace using_chain_09; }
namespace using_chain_11 { using namespace using_chain_10; }
namespace using_chain_12 { using namespace using_chain_11; }
namespace using_chain_13 { using namespace using_chain_12; }
namespace using_chain_14 { using namespace using_chain_13; }
namespace using_chain_15 { using namespace using_chain_14; }
namespace using_chain_16 { using namespace using_chain_15; }
namespace using_chain_17 { using namespace using_chain_16; }
namespace using_chain_18 { using namespace using_chain_17; }
namespace using_chain_19 { using namespace using_chain_18; }
namespace using_chain_20 { using namespace using_chain_19; }
namespace using_chain_21 { using namespace using_chain_20; }
namespace using_chain_22 { using namespace using_chain_21; }
namespace using_chain_23 { using namespace using_chain_22; }
namespace using_chain_24 { using namespace using_chain_23; }
namespace using_chain_25 { using namespace using_chain_24; }
namespace using_chain_26 { using namespace using_chain_25; }
namespace using_chain_27 { using namespace using_chain_26; }
namespace using_chain_28 { using namespace using_chain_27; }
namespace using_chain_29 { using namespace using_chain_28; }
namespace using_chain_30 { using namespace using_chain_29; }
namespace using_chain_31 { using namespace using_chain_30; }
namespace using_chain_32 { using namespace using_chain_31; }
namespace using_chain_33 { using namespace using_chain_32; }
namespace using_chain_34 { using namespace using_chain_33; }
namespace using_chain_35 { using namespace using_chain_34; }
namespace using_chain_36 { using namespace using_chain_35; }
namespace using_chain_37 { using namespace using_chain_36; }
namespace using_chain_38 { using namespace using_chain_37; }
namespace using_chain_39 { using namespace using_chain_38; }
namespace using_chain_40 { using namespace using_chain_39; inline int read() { return value; } }

using DeepOwnerType = namespace_component_000::namespace_component_001::namespace_component_002::namespace_component_003::namespace_component_004::namespace_component_005::namespace_component_006::namespace_component_007::namespace_component_008::namespace_component_009::namespace_component_010::namespace_component_011::namespace_component_012::namespace_component_013::namespace_component_014::namespace_component_015::namespace_component_016::namespace_component_017::namespace_component_018::namespace_component_019::namespace_component_020::namespace_component_021::namespace_component_022::namespace_component_023::namespace_component_024::namespace_component_025::namespace_component_026::namespace_component_027::namespace_component_028::namespace_component_029::namespace_component_030::namespace_component_031::namespace_component_032::namespace_component_033::DeepOwner;

int main() {
    DeepOwnerType object{};
    auto member = &namespace_component_000::namespace_component_001::namespace_component_002::namespace_component_003::namespace_component_004::namespace_component_005::namespace_component_006::namespace_component_007::namespace_component_008::namespace_component_009::namespace_component_010::namespace_component_011::namespace_component_012::namespace_component_013::namespace_component_014::namespace_component_015::namespace_component_016::namespace_component_017::namespace_component_018::namespace_component_019::namespace_component_020::namespace_component_021::namespace_component_022::namespace_component_023::namespace_component_024::namespace_component_025::namespace_component_026::namespace_component_027::namespace_component_028::namespace_component_029::namespace_component_030::namespace_component_031::namespace_component_032::namespace_component_033::DeepOwner::value;
    object.*member = 42;
    return object.value == 42 &&
        namespace_component_000::namespace_component_001::namespace_component_002::namespace_component_003::namespace_component_004::namespace_component_005::namespace_component_006::namespace_component_007::namespace_component_008::namespace_component_009::namespace_component_010::namespace_component_011::namespace_component_012::namespace_component_013::namespace_component_014::namespace_component_015::namespace_component_016::namespace_component_017::namespace_component_018::namespace_component_019::namespace_component_020::namespace_component_021::namespace_component_022::namespace_component_023::namespace_component_024::namespace_component_025::namespace_component_026::namespace_component_027::namespace_component_028::namespace_component_029::namespace_component_030::namespace_component_031::namespace_component_032::namespace_component_033::DeepOwner::marker() == 17 && using_chain_40::value == 29 && using_chain_40::read() == 29 && namespace_component_000::namespace_component_001::namespace_component_002::namespace_component_003::namespace_component_004::namespace_component_005::namespace_component_006::namespace_component_007::namespace_component_008::namespace_component_009::namespace_component_010::namespace_component_011::namespace_component_012::namespace_component_013::namespace_component_014::namespace_component_015::namespace_component_016::namespace_component_017::namespace_component_018::namespace_component_019::namespace_component_020::namespace_component_021::namespace_component_022::namespace_component_023::namespace_component_024::namespace_component_025::namespace_component_026::namespace_component_027::namespace_component_028::namespace_component_029::namespace_component_030::namespace_component_031::namespace_component_032::namespace_component_033::run_template_template_probe() == 1
        ? 0 : 1;
}
