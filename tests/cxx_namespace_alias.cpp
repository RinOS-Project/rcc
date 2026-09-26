namespace library {
namespace v2 {
struct Value {
    int value;
};

int plus_one(int value) {
    return value + 1;
}
}
}

namespace api = library::v2;
namespace api2 = api;
namespace local {
namespace api = library::v2;
}

int main() {
    api::Value first = {4};
    api2::Value second = {6};
    local::api::Value third = {8};
    return api::plus_one(first.value) +
                   api2::plus_one(second.value) +
                   local::api::plus_one(third.value) == 21 ? 0 : 1;
}
