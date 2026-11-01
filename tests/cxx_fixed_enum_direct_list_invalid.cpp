enum class Byte : unsigned char {};
enum Plain { plain_zero };

int runtime_integer();

Byte too_large{256};
Byte negative{-1};
Byte runtime_value{runtime_integer()};
Byte floating_value{1.0};
Plain integer_to_nonfixed_enum{1};
Byte multiple_elements{1, 2};
Byte copy_list = {1};
