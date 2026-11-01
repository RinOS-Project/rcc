enum class Byte : unsigned char {};
enum SignedByte : signed char {};
enum class IntCode : int {};
enum Plain { plain_zero };

int runtime_integer();
long long runtime_long_long();

Byte too_large{256};
Byte negative{-1};
Byte runtime_value{runtime_integer()};
Byte floating_value{1.0};
SignedByte signed_too_large{128};
SignedByte unsigned_to_signed{255u};
IntCode wide_too_large{2147483648LL};
IntCode runtime_wide{runtime_long_long()};
Plain integer_to_nonfixed_enum{1};
Byte multiple_elements{1, 2};
Byte copy_list = {1};
