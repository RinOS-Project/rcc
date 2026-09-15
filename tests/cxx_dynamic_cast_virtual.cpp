class VirtualBase {
public:
    virtual int value() { return 7; }
};

class Middle : public virtual VirtualBase {
public:
    virtual int middle() { return 11; }
};

class Most : public Middle {
public:
    int value() override { return 42; }
};

extern "C" int main() {
    Most most{};
    Middle* middle = &most;
    VirtualBase* virtual_base = dynamic_cast<VirtualBase*>(middle);
    Most* recovered = dynamic_cast<Most*>(virtual_base);
    return virtual_base && virtual_base->value() == 42 &&
           recovered == &most && middle->middle() == 11 ? 0 : 1;
}
