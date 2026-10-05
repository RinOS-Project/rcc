struct EmptyNoUniqueAddress {};

struct NoUniqueAddressHolder {
    [[no_unique_address]] EmptyNoUniqueAddress empty;
    int value;
};

struct NonEmptyNoUniqueAddress {
    int value;
};

struct NoUniqueAddressKeepsStorage {
    [[no_unique_address]] NonEmptyNoUniqueAddress first;
    int second;
};

struct SameTypeNoUniqueAddress {
    [[no_unique_address]] EmptyNoUniqueAddress first;
    EmptyNoUniqueAddress second;
    int value;
};

static_assert(sizeof(NoUniqueAddressHolder) == sizeof(int),
              "an empty no_unique_address member can share the object offset");
static_assert(sizeof(NoUniqueAddressKeepsStorage) == 2 * sizeof(int),
              "non-empty no_unique_address members retain ordinary storage");
static_assert(sizeof(SameTypeNoUniqueAddress) >= 2 * sizeof(int),
              "same-type empty members must not collapse the object");

int main() {
    NoUniqueAddressHolder value;
    value.value = 42;
    return value.value == 42 ? 0 : 1;
}
