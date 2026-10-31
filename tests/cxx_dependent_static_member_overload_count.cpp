template<int Value, typename Owner, typename... Tail>
struct StaticMemberCallBox {};
template<int Value> struct StaticMemberCallArgument {
    int values[Value + 1];
};
struct StaticMemberCallOwner {
    static constexpr int choose(StaticMemberCallArgument<0>*) { return 0; }
    static constexpr int choose(StaticMemberCallArgument<1>*) { return 1; }
    static constexpr int choose(StaticMemberCallArgument<2>*) { return 2; }
    static constexpr int choose(StaticMemberCallArgument<3>*) { return 3; }
    static constexpr int choose(StaticMemberCallArgument<4>*) { return 4; }
    static constexpr int choose(StaticMemberCallArgument<5>*) { return 5; }
    static constexpr int choose(StaticMemberCallArgument<6>*) { return 6; }
    static constexpr int choose(StaticMemberCallArgument<7>*) { return 7; }
    static constexpr int choose(StaticMemberCallArgument<8>*) { return 8; }
    static constexpr int choose(StaticMemberCallArgument<9>*) { return 9; }
    static constexpr int choose(StaticMemberCallArgument<10>*) { return 10; }
    static constexpr int choose(StaticMemberCallArgument<11>*) { return 11; }
    static constexpr int choose(StaticMemberCallArgument<12>*) { return 12; }
    static constexpr int choose(StaticMemberCallArgument<13>*) { return 13; }
    static constexpr int choose(StaticMemberCallArgument<14>*) { return 14; }
    static constexpr int choose(StaticMemberCallArgument<15>*) { return 15; }
    static constexpr int choose(StaticMemberCallArgument<16>*) { return 16; }
    static constexpr int choose(StaticMemberCallArgument<17>*) { return 17; }
    static constexpr int choose(StaticMemberCallArgument<18>*) { return 18; }
    static constexpr int choose(StaticMemberCallArgument<19>*) { return 19; }
    static constexpr int choose(StaticMemberCallArgument<20>*) { return 20; }
    static constexpr int choose(StaticMemberCallArgument<21>*) { return 21; }
    static constexpr int choose(StaticMemberCallArgument<22>*) { return 22; }
    static constexpr int choose(StaticMemberCallArgument<23>*) { return 23; }
    static constexpr int choose(StaticMemberCallArgument<24>*) { return 24; }
    static constexpr int choose(StaticMemberCallArgument<25>*) { return 25; }
    static constexpr int choose(StaticMemberCallArgument<26>*) { return 26; }
    static constexpr int choose(StaticMemberCallArgument<27>*) { return 27; }
    static constexpr int choose(StaticMemberCallArgument<28>*) { return 28; }
    static constexpr int choose(StaticMemberCallArgument<29>*) { return 29; }
    static constexpr int choose(StaticMemberCallArgument<30>*) { return 30; }
    static constexpr int choose(StaticMemberCallArgument<31>*) { return 31; }
    static constexpr int choose(StaticMemberCallArgument<32>*) { return 32; }
};
template<typename Owner, typename... Tail>
int select_static_member_call(
    StaticMemberCallBox<
        Owner::choose((StaticMemberCallArgument<32>*)nullptr), Owner,
        Tail...>) {
    return 41;
}
template<typename Owner, typename... Tail>
int select_static_member_call(StaticMemberCallBox<31, Owner, Tail...>) {
    return 42;
}
int main() {
    return select_static_member_call(
               StaticMemberCallBox<32, StaticMemberCallOwner, int>{}) == 41
        ? 0 : 1;
}
