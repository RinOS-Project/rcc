struct MemberFunctionOwner {
    int value;

    int add(int amount) {
        return value + amount;
    }

    int scale(int factor) {
        return value * factor;
    }
};

static int invoke_dot(MemberFunctionOwner& object,
                      int (MemberFunctionOwner::*method)(int), int value) {
    return (object.*method)(value);
}

static int invoke_arrow(MemberFunctionOwner* object,
                        int (MemberFunctionOwner::*method)(int), int value) {
    return (object->*method)(value);
}

int main() {
    MemberFunctionOwner object;
    int (MemberFunctionOwner::*method)(int);

    object.value = 7;
    method = &MemberFunctionOwner::add;
    if (invoke_dot(object, method, 5) != 12) return 1;
    if (invoke_arrow(&object, method, 9) != 16) return 2;

    method = &MemberFunctionOwner::scale;
    if (invoke_dot(object, method, 3) != 21) return 3;
    if (invoke_arrow(&object, method, 4) != 28) return 4;
    return 0;
}
