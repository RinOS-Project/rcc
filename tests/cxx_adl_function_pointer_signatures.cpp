namespace return_association {
struct value {
    int data;

    friend int probe(value (*callback)(int)) {
        return callback(5).data == 17 ? 0 : 1;
    }
};

value produce(int input) {
    return value{input + 12};
}
}

namespace parameter_association {
struct derived;

struct base {
    friend int probe(int (**callback)(derived));
};

struct derived : base {
    int data;
};

int consume(derived input) {
    return input.data;
}

int probe(int (**callback)(derived)) {
    derived input{base{}, 19};
    return (*callback)(input) == 19 ? 0 : 1;
}
}

namespace member_pointer_association {
struct owner;

struct value {
    int data;

    friend int probe(value (owner::*callback)(int) const);
};

struct owner {
    value transform(int input) const {
        return value{input + 18};
    }
};

int probe(value (owner::*callback)(int) const) {
    owner object;
    return (object.*callback)(5).data == 23 ? 0 : 1;
}
}

int main() {
    int (*consume_function)(parameter_association::derived) =
        &parameter_association::consume;
    return probe(&return_association::produce) +
           probe(&consume_function) +
           probe(&member_pointer_association::owner::transform);
}
