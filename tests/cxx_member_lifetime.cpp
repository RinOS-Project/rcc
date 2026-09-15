class MemberLifetimeLeaf {
public:
    explicit MemberLifetimeLeaf(int* value) : value_(value) {}

    int* value() {
        return value_;
    }

    ~MemberLifetimeLeaf() {
        *value_ = *value_ * 10 + 1;
    }

private:
    int* value_;
};

class MemberLifetimeOuter {
public:
    MemberLifetimeOuter(int* first, int* second)
        : first_(first), second_(second) {}

    ~MemberLifetimeOuter() {
        *first_.value() = *first_.value() * 10 + 3;
    }

private:
    MemberLifetimeLeaf first_;
    MemberLifetimeLeaf second_;
};

int member_lifetime_local() {
    int events = 0;
    {
        MemberLifetimeOuter value(&events, &events);
    }
    return events == 311 ? 0 : 1;
}

int member_lifetime_heap() {
    int events = 0;
    MemberLifetimeOuter* value = new MemberLifetimeOuter(&events, &events);
    delete value;
    return events == 311 ? 0 : 1;
}

int main() {
    return member_lifetime_local() + member_lifetime_heap();
}
