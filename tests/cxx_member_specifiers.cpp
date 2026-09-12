class member_probe final {
public:
    constexpr member_probe() noexcept : value_(0) {}
    explicit member_probe(int value) noexcept : value_(value) {}
    member_probe(const member_probe&) = delete;
    member_probe(member_probe&&) noexcept = default;
    member_probe& operator=(const member_probe&) = delete;
    virtual ~member_probe() noexcept = default;

    [[nodiscard]] constexpr int value() const noexcept { return value_; }
    constexpr explicit operator bool() const noexcept { return value_ != 0; }

private:
    int value_;
};
