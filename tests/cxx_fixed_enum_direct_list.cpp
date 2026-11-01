enum class Byte : unsigned char {};
enum SignedByte : signed char {};
enum class IntCode : int {};

constexpr Byte global_byte{201};
constexpr SignedByte global_signed_byte{-101};

static unsigned char dynamic_byte() {
    return 23;
}

static int dynamic_int() {
    return 29;
}

static Byte make_byte() {
    return Byte{37};
}

int main() {
    Byte zero{};
    Byte low{0};
    Byte high{255};
    Byte from_bool{true};
    Byte from_dynamic_underlying{dynamic_byte()};
    SignedByte signed_low{-128};
    SignedByte signed_high{127};
    IntCode from_dynamic_int{dynamic_int()};
    Byte same_type{global_byte};
    Byte from_direct_expression = Byte{42};

    return static_cast<int>(zero) != 0 ||
           static_cast<int>(low) != 0 ||
           static_cast<int>(high) != 255 ||
           static_cast<int>(from_bool) != 1 ||
           static_cast<int>(from_dynamic_underlying) != 23 ||
           static_cast<int>(signed_low) != -128 ||
           static_cast<int>(signed_high) != 127 ||
           static_cast<int>(from_dynamic_int) != 29 ||
           static_cast<int>(same_type) != 201 ||
           static_cast<int>(global_signed_byte) != -101 ||
           static_cast<int>(from_direct_expression) != 42 ||
           static_cast<int>(make_byte()) != 37;
}
