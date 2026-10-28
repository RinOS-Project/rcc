struct SmallIntegerLike {
    unsigned char value;

    operator int() const {
        return value;
    }
};

struct ConstrainedMemberComparison {
    template <typename T>
        requires (sizeof(T) == sizeof(int))
    bool operator<(const T&) const {
        return true;
    }
};

int reject_stale_member_template_specialization(
    ConstrainedMemberComparison value, SmallIntegerLike small) {
    value < 1;
    return value < small;
}
