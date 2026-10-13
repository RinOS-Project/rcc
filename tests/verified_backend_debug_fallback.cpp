extern "C" int verified_debug_fallback_prefix(int value)
{
    int local = value + 3;
    {
        int nested = local + 1;
        return nested;
    }
}

struct VerifiedDebugFallbackBase {
    int* events;
    int value;

    VerifiedDebugFallbackBase(int* event_log, int item_value)
        : events(event_log), value(item_value) {}

    ~VerifiedDebugFallbackBase() { *events += 1; }
};

struct VerifiedDebugFallbackDerived : VerifiedDebugFallbackBase {
    VerifiedDebugFallbackDerived(int* event_log, int item_value)
        : VerifiedDebugFallbackBase(event_log, item_value) {}

    ~VerifiedDebugFallbackDerived() { *events += 2; }
};

struct VerifiedDebugFallbackSource {
    int* events;
    int value;

    operator VerifiedDebugFallbackDerived() const {
        return {events, value};
    }
};

extern "C" int verified_debug_fallback_conversion(int* events, int value)
{
    const VerifiedDebugFallbackBase& converted =
        VerifiedDebugFallbackSource{events, value};
    return converted.value;
}
