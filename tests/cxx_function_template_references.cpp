template<typename T>
int read_const_reference(const T& value) {
    return value;
}

template<typename T>
int update_lvalue(T& value) {
    value += 3;
    return value;
}

template<typename T>
int read_rvalue(T&& value) {
    return value;
}

template<typename T>
T copy_from_const_pointer(const T* value) {
    T copy = *value;
    copy += 1;
    return copy;
}

int double_value(int value) {
    return value + value;
}

int select_reference_overload(int&) {
    return 1;
}

int select_reference_overload(int&&) {
    return 2;
}

int select_reference_overload(const int&) {
    return 3;
}

int comma_calls;

int mark_comma() {
    comma_calls += 1;
    return 0;
}

int update_rvalue_reference_local(int&& value) {
    int&& local = static_cast<int&&>(value);
    local += 2;
    return value;
}

int& return_lvalue_reference(int& value) {
    return static_cast<int&>(value);
}

int&& return_rvalue_reference(int&& value) {
    return static_cast<int&&>(value);
}

int& select_conditional_lvalue(bool choose_first, int& first, int& second) {
    return choose_first ? first : second;
}

int&& select_conditional_xvalue(bool choose_first, int&& first, int&& second) {
    return choose_first ? static_cast<int&&>(first)
                        : static_cast<int&&>(second);
}

int& return_comma_lvalue(int& value) {
    return (mark_comma(), value);
}

int&& return_comma_xvalue(int&& value) {
    return (mark_comma(), static_cast<int&&>(value));
}

struct ReferenceMemberValue {
    int value;
};

struct LifetimeExtendedTemporary {
    int* events;
    int value;

    ~LifetimeExtendedTemporary() {
        *events = *events * 10 + value;
    }
};

struct MemberReceiverLifetime {
    int* events;
    int value;
    int call_marker;
    int destructor_marker;

    int read() const {
        *events = *events * 10 + call_marker;
        return value;
    }

    ~MemberReceiverLifetime() {
        *events = *events * 10 + destructor_marker;
    }
};

MemberReceiverLifetime make_member_receiver_lifetime(int* events, int value) {
    return {events, value, 2, 1};
}

struct LifetimeExtendedBase {
    int* events;
    int value;

    LifetimeExtendedBase(int* object_events, int object_value)
        : events(object_events), value(object_value) {}

    ~LifetimeExtendedBase() {
        *events = *events * 10 + 1;
    }
};

struct LifetimeExtendedDerived : LifetimeExtendedBase {
    LifetimeExtendedDerived(int* events, int value)
        : LifetimeExtendedBase(events, value) {}

    ~LifetimeExtendedDerived() {
        *events = *events * 10 + 2;
    }
};

struct ConversionLifetimeDerived : LifetimeExtendedBase {
    ConversionLifetimeDerived(int* events, int value)
        : LifetimeExtendedBase(events, value) {}

    ~ConversionLifetimeDerived() {
        *events = *events * 10 + 2;
    }
};

struct ConversionTemporarySource {
    int* events;
    int value;

    operator LifetimeExtendedTemporary() const {
        return {events, value};
    }
};

struct ConversionDerivedSource {
    int* events;
    int value;

    operator ConversionLifetimeDerived() const {
        return {events, value};
    }
};

struct ConversionReferenceValue {
    int value;
};

struct ConversionReferenceBase {
    int value;
};

struct ConversionReferencePrefix {
    int prefix;
};

struct ConversionReferenceDerived : ConversionReferencePrefix,
                                    ConversionReferenceBase {
    int derived_value;
};

struct ConversionLvalueSource {
    ConversionReferenceValue* value;

    operator ConversionReferenceValue&() {
        return *value;
    }
};

struct ConversionXvalueSource {
    ConversionReferenceValue* value;

    operator ConversionReferenceValue&&() {
        return static_cast<ConversionReferenceValue&&>(*value);
    }
};

struct ConversionDerivedXvalueSource {
    ConversionReferenceDerived* value;

    operator ConversionReferenceDerived&&() {
        return static_cast<ConversionReferenceDerived&&>(*value);
    }
};

ConversionReferenceDerived global_conversion_reference_derived{
    {73}, {74}, 75};

int read_conversion_lvalue(const ConversionReferenceValue& value) {
    return value.value;
}

int read_conversion_xvalue(ConversionReferenceValue&& value) {
    return value.value;
}

int read_conversion_base_xvalue(ConversionReferenceBase&& value) {
    return value.value;
}

ConversionReferenceBase&& return_conversion_base_xvalue(
    ConversionDerivedXvalueSource& source) {
    return source;
}

struct LifetimeInheritedCleanup : LifetimeExtendedBase {
    LifetimeInheritedCleanup(int* events, int value)
        : LifetimeExtendedBase(events, value) {}
};

struct LifetimeVirtualBase {
    int* events;
    int value;

    LifetimeVirtualBase(int* object_events, int object_value)
        : events(object_events), value(object_value) {}

    ~LifetimeVirtualBase() {
        *events = *events * 10 + 1;
    }
};

struct LifetimeVirtualDerived : virtual LifetimeVirtualBase {
    LifetimeVirtualDerived(int* events, int value)
        : LifetimeVirtualBase(events, value) {}

    ~LifetimeVirtualDerived() {
        *events = *events * 10 + 2;
    }
};

int lifetime_virtual_diamond_cleanup_events = 0;

struct LifetimeVirtualDiamondBase {
    int* events;

    explicit LifetimeVirtualDiamondBase(int* object_events)
        : events(object_events) {}

    ~LifetimeVirtualDiamondBase() {
        *events = *events * 10 + 1;
        lifetime_virtual_diamond_cleanup_events =
                lifetime_virtual_diamond_cleanup_events * 10 + 1;
    }
};

struct LifetimeVirtualDiamondLeft : virtual LifetimeVirtualDiamondBase {
    LifetimeVirtualDiamondLeft() : LifetimeVirtualDiamondBase(0) {}

    ~LifetimeVirtualDiamondLeft() {
        *events = *events * 10 + 2;
        lifetime_virtual_diamond_cleanup_events =
                lifetime_virtual_diamond_cleanup_events * 10 + 2;
    }
};

struct LifetimeVirtualDiamondRight : virtual LifetimeVirtualDiamondBase {
    LifetimeVirtualDiamondRight() : LifetimeVirtualDiamondBase(0) {}

    ~LifetimeVirtualDiamondRight() {
        *events = *events * 10 + 3;
        lifetime_virtual_diamond_cleanup_events =
                lifetime_virtual_diamond_cleanup_events * 10 + 3;
    }
};

struct LifetimeVirtualDiamondDerived : LifetimeVirtualDiamondLeft,
                                       LifetimeVirtualDiamondRight {
    explicit LifetimeVirtualDiamondDerived(int* object_events)
        : LifetimeVirtualDiamondBase(object_events),
          LifetimeVirtualDiamondLeft(), LifetimeVirtualDiamondRight() {}

    ~LifetimeVirtualDiamondDerived() {
        *events = *events * 10 + 4;
        lifetime_virtual_diamond_cleanup_events =
                lifetime_virtual_diamond_cleanup_events * 10 + 4;
    }
};

int* lifetime_virtual_diamond_left_events(
        LifetimeVirtualDiamondLeft* object) {
    return object->events;
}

int* lifetime_virtual_diamond_right_events(
        LifetimeVirtualDiamondRight* object) {
    return object->events;
}

int read_virtual_diamond_xvalue(const LifetimeVirtualDiamondBase&& object) {
    return object.events != 0;
}

LifetimeExtendedTemporary make_lifetime_extended_temporary(int* events,
                                                           int value) {
    return {events, value};
}

int read_lifetime_temporary_argument(
        const LifetimeExtendedTemporary& value) {
    return value.value;
}

int read_lifetime_temporary_rvalue_argument(
        LifetimeExtendedTemporary&& value) {
    return value.value;
}

int read_lifetime_base_argument(const LifetimeExtendedBase& value) {
    return value.value;
}

int read_conditional_lifetime_argument(bool choose_first, int* events) {
    return read_lifetime_temporary_argument(
        choose_first
            ? make_lifetime_extended_temporary(events, 8)
            : make_lifetime_extended_temporary(events, 9));
}

int read_comma_lifetime_argument(int* events) {
    return read_lifetime_temporary_argument(
        (mark_comma(), make_lifetime_extended_temporary(events, 10)));
}

int conditional_lifetime_argument_branch(bool choose, int* events) {
    return choose
        ? read_lifetime_temporary_argument(
              make_lifetime_extended_temporary(events, 11))
        : 0;
}

int read_lifetime_temporary_and_return(int* events, int value) {
    return read_lifetime_temporary_argument(
        make_lifetime_extended_temporary(events, value));
}

int&& return_member_xvalue(ReferenceMemberValue&& object) {
    return static_cast<ReferenceMemberValue&&>(object).value;
}

decltype(auto) return_member_xvalue_auto(ReferenceMemberValue&& object) {
    return (static_cast<ReferenceMemberValue&&>(object).value);
}

decltype(auto) select_conditional_decltype_xvalue(
        bool choose_first, int&& first, int&& second) {
    return (choose_first ? static_cast<int&&>(first)
                         : static_cast<int&&>(second));
}

template<typename R, typename A>
R invoke(R (*function)(A), A value) {
    return function(value);
}

template<typename T, typename U = T>
U default_type_copy(T value) {
    U result{};
    result = value;
    return result;
}

template<typename T>
int read_array_element(T* value) {
    return value[1];
}

int main() {
    const int constant = 7;
    int mutable_value = 4;
    int values[2] = {5, 6};
    int overload_lvalue = 50;
    const int overload_const_lvalue = 60;
    if (select_reference_overload(overload_lvalue) != 1) return 30;
    if (select_reference_overload(51) != 2) return 31;
    if (select_reference_overload(overload_const_lvalue) != 3) return 32;
    if (select_reference_overload(
            static_cast<int&&>(overload_lvalue)) != 2) return 33;
    if (read_const_reference(constant) != 7) return 1;
    if (update_lvalue(mutable_value) != 7) return 2;
    if (mutable_value != 7) return 3;
    if (update_rvalue_reference_local(
            static_cast<int&&>(mutable_value)) != 9) return 4;
    if (mutable_value != 9) return 5;
    int& lvalue_result = return_lvalue_reference(mutable_value);
    lvalue_result += 1;
    if (mutable_value != 10) return 6;
    int&& rvalue_result = return_rvalue_reference(
            static_cast<int&&>(mutable_value));
    rvalue_result += 2;
    if (mutable_value != 12) return 7;
    int other_value = 20;
    int& selected_lvalue = select_conditional_lvalue(
            false, mutable_value, other_value);
    selected_lvalue += 1;
    if (mutable_value != 12 || other_value != 21) return 8;
    int& comma_lvalue = return_comma_lvalue(other_value);
    comma_lvalue += 1;
    if (other_value != 22 || comma_calls != 1) return 23;
    char small_first = 'a';
    char small_second = 'b';
    (true ? small_first : small_second) = 'z';
    if (small_first != 'z' || small_second != 'b') return 9;
    int&& selected_xvalue = select_conditional_xvalue(
            true, static_cast<int&&>(mutable_value),
            static_cast<int&&>(other_value));
    selected_xvalue += 2;
    if (mutable_value != 14 || other_value != 22) return 10;
    int&& comma_xvalue = return_comma_xvalue(
            static_cast<int&&>(other_value));
    comma_xvalue += 1;
    if (other_value != 23 || comma_calls != 2) return 24;
    decltype(auto) deduced_xvalue = select_conditional_decltype_xvalue(
            false, static_cast<int&&>(mutable_value),
            static_cast<int&&>(other_value));
    deduced_xvalue += 3;
    if (mutable_value != 14 || other_value != 26) return 11;
    ReferenceMemberValue member_value{};
    member_value.value = 25;
    int&& member_result = return_member_xvalue(
            static_cast<ReferenceMemberValue&&>(member_value));
    member_result += 4;
    if (member_value.value != 29) return 12;
    decltype(auto) member_auto_result = return_member_xvalue_auto(
            static_cast<ReferenceMemberValue&&>(member_value));
    member_auto_result += 2;
    if (member_value.value != 31) return 13;
    int auto_lvalue_value = 32;
    auto&& deduced_lvalue_reference = auto_lvalue_value;
    deduced_lvalue_reference += 1;
    if (auto_lvalue_value != 33) return 20;
    int auto_xvalue_value = 42;
    auto&& deduced_xvalue_reference =
            static_cast<int&&>(auto_xvalue_value);
    deduced_xvalue_reference += 2;
    if (auto_xvalue_value != 44) return 21;
    int temporary_lifetime_events = 0;
    {
        const LifetimeExtendedTemporary& extended_object =
                make_lifetime_extended_temporary(
                        &temporary_lifetime_events, 5);
        if (extended_object.value != 5 || temporary_lifetime_events != 0)
            return 25;
        if (double_value(4) != 8 || temporary_lifetime_events != 0)
            return 26;
        LifetimeExtendedTemporary const& trailing_cv_object =
                make_lifetime_extended_temporary(
                        &temporary_lifetime_events, 6);
        if (trailing_cv_object.value != 6 ||
            temporary_lifetime_events != 0)
            return 28;
        if (double_value(3) != 6 || temporary_lifetime_events != 0)
            return 29;
    }
    if (temporary_lifetime_events != 65) return 27;
    int argument_temporary_events = 0;
    int argument_temporary_result =
        read_lifetime_temporary_argument(
            make_lifetime_extended_temporary(
                &argument_temporary_events, 4)) +
        (argument_temporary_events == 0 ? 1 : 0);
    if (argument_temporary_result != 5 || argument_temporary_events != 4)
        return 66;
    argument_temporary_events = 0;
    int rvalue_argument_result = read_lifetime_temporary_rvalue_argument(
        make_lifetime_extended_temporary(&argument_temporary_events, 5));
    if (rvalue_argument_result != 5 || argument_temporary_events != 5)
        return 67;
    int base_argument_temporary_events = 0;
    int base_argument_result = read_lifetime_base_argument(
        LifetimeExtendedDerived{&base_argument_temporary_events, 6});
    if (base_argument_result != 6 || base_argument_temporary_events != 21)
        return 68;
    argument_temporary_events = 0;
    read_lifetime_temporary_argument(make_lifetime_extended_temporary(
        &argument_temporary_events, 7));
    if (argument_temporary_events != 7) return 69;
    argument_temporary_events = 0;
    int compound_temporary_argument_result =
        read_lifetime_temporary_argument(LifetimeExtendedTemporary{
            &argument_temporary_events, 10});
    if (compound_temporary_argument_result != 10 ||
        argument_temporary_events != 10)
        return 72;
    argument_temporary_events = 0;
    if (read_lifetime_temporary_argument(make_lifetime_extended_temporary(
            &argument_temporary_events, 8))) {
        if (argument_temporary_events != 8) return 70;
    }
    argument_temporary_events = 0;
    int returned_temporary_argument = read_lifetime_temporary_and_return(
        &argument_temporary_events, 9);
    if (returned_temporary_argument != 9 || argument_temporary_events != 9)
        return 71;
    argument_temporary_events = 0;
    if (read_conditional_lifetime_argument(
            true, &argument_temporary_events) != 8 ||
        argument_temporary_events != 8)
        return 74;
    argument_temporary_events = 0;
    if (read_conditional_lifetime_argument(
            false, &argument_temporary_events) != 9 ||
        argument_temporary_events != 9)
        return 75;
    argument_temporary_events = 0;
    comma_calls = 0;
    if (read_comma_lifetime_argument(&argument_temporary_events) != 10 ||
        argument_temporary_events != 10 || comma_calls != 1)
        return 76;
    argument_temporary_events = 0;
    if (conditional_lifetime_argument_branch(
            false, &argument_temporary_events) != 0 ||
        argument_temporary_events != 0)
        return 77;
    argument_temporary_events = 0;
    if (conditional_lifetime_argument_branch(
            true, &argument_temporary_events) != 11 ||
        argument_temporary_events != 11)
        return 78;
    int member_receiver_events = 0;
    int member_receiver_result = make_member_receiver_lifetime(
        &member_receiver_events, 12).read();
    if (member_receiver_result != 12 || member_receiver_events != 21)
        return 79;
    member_receiver_events = 0;
    int compound_member_receiver_result = MemberReceiverLifetime{
        &member_receiver_events, 13, 3, 4}.read();
    if (compound_member_receiver_result != 13 ||
        member_receiver_events != 34)
        return 80;
    int converted_temporary_events = 0;
    {
        const LifetimeExtendedTemporary& converted_temporary =
            ConversionTemporarySource{&converted_temporary_events, 7};
        if (converted_temporary.value != 7 ||
            converted_temporary_events != 0)
            return 57;
        if (double_value(5) != 10 || converted_temporary_events != 0)
            return 58;
    }
    if (converted_temporary_events != 7) return 59;
    int converted_derived_events = 0;
    {
        const LifetimeExtendedBase& converted_derived =
            ConversionDerivedSource{&converted_derived_events, 8};
        if (converted_derived.value != 8 || converted_derived_events != 0)
            return 60;
        if (double_value(6) != 12 || converted_derived_events != 0)
            return 61;
    }
    if (converted_derived_events != 21) return 62;
    ConversionReferenceValue converted_reference_value{63};
    ConversionLvalueSource conversion_lvalue_source{
        &converted_reference_value};
    const ConversionReferenceValue& converted_lvalue =
        conversion_lvalue_source;
    if (&converted_lvalue != &converted_reference_value ||
        read_conversion_lvalue(conversion_lvalue_source) != 63)
        return 63;
    ConversionXvalueSource conversion_xvalue_source{
        &converted_reference_value};
    ConversionReferenceValue&& converted_xvalue =
        conversion_xvalue_source;
    if (&converted_xvalue != &converted_reference_value ||
        read_conversion_xvalue(conversion_xvalue_source) != 63)
        return 64;
    converted_xvalue.value = 65;
    if (converted_reference_value.value != 65) return 65;
    ConversionReferenceDerived converted_derived_xvalue_value{
        {65}, {66}, 67};
    if (converted_derived_xvalue_value.value != 66) return 71;
    if (converted_derived_xvalue_value.derived_value != 67 ||
        converted_derived_xvalue_value.prefix != 65) return 72;
    ConversionDerivedXvalueSource converted_derived_xvalue_source{
        &converted_derived_xvalue_value};
    ConversionReferenceBase&& converted_derived_xvalue =
        converted_derived_xvalue_source;
    ConversionReferenceBase* expected_converted_base =
        (ConversionReferenceBase*)((char*)&converted_derived_xvalue_value +
                                   sizeof(ConversionReferencePrefix));
    if (&converted_derived_xvalue != expected_converted_base) return 66;
    if (converted_derived_xvalue.value != 66) return 69;
    if (read_conversion_base_xvalue(converted_derived_xvalue_source) != 66)
        return 70;
    ConversionReferenceBase&& returned_converted_derived_xvalue =
        return_conversion_base_xvalue(converted_derived_xvalue_source);
    if (&returned_converted_derived_xvalue !=
            static_cast<ConversionReferenceBase*>(
                &converted_derived_xvalue_value) ||
        returned_converted_derived_xvalue.value != 66)
        return 67;
    returned_converted_derived_xvalue.value = 68;
    if (converted_derived_xvalue_value.value != 68) return 68;
    if (global_conversion_reference_derived.prefix != 73 ||
        global_conversion_reference_derived.value != 74 ||
        global_conversion_reference_derived.derived_value != 75) return 73;
    ConversionReferenceDerived parenthesized_derived_xvalue(
        ConversionReferencePrefix{76}, ConversionReferenceBase{77}, 78);
    if (parenthesized_derived_xvalue.prefix != 76 ||
        parenthesized_derived_xvalue.value != 77 ||
        parenthesized_derived_xvalue.derived_value != 78) return 74;
    int derived_temporary_events = 0;
    {
        const LifetimeExtendedBase& extended_base =
                LifetimeExtendedDerived{&derived_temporary_events, 7};
        if (extended_base.value != 7 || derived_temporary_events != 0)
            return 34;
        if (double_value(5) != 10 || derived_temporary_events != 0)
            return 35;
    }
    if (derived_temporary_events != 21) return 36;
    int cast_derived_temporary_events = 0;
    {
        const LifetimeExtendedBase&& cast_extended_base =
                static_cast<const LifetimeExtendedBase&&>(
                    LifetimeExtendedDerived{
                        &cast_derived_temporary_events, 8});
        if (cast_extended_base.value != 8 ||
            cast_derived_temporary_events != 0)
            return 50;
        if (double_value(2) != 4 || cast_derived_temporary_events != 0)
            return 51;
    }
    if (cast_derived_temporary_events != 21) return 52;
    int inherited_cleanup_events = 0;
    {
        LifetimeInheritedCleanup inherited_object(
                &inherited_cleanup_events, 9);
        if (inherited_object.value != 9 || inherited_cleanup_events != 0)
            return 37;
    }
    if (inherited_cleanup_events != 1) return 38;
    int virtual_temporary_events = 0;
    {
        const LifetimeVirtualBase& virtual_base =
                LifetimeVirtualDerived{&virtual_temporary_events, 10};
        if (virtual_base.value != 10 || virtual_temporary_events != 0)
            return 39;
        if (double_value(6) != 12 || virtual_temporary_events != 0)
            return 40;
    }
    if (virtual_temporary_events != 21) return 41;
    lifetime_virtual_diamond_cleanup_events = 0;
    int cast_virtual_diamond_events = 0;
    {
        const LifetimeVirtualDiamondBase&& cast_diamond_base =
                static_cast<const LifetimeVirtualDiamondBase&&>(
                    LifetimeVirtualDiamondDerived{
                        &cast_virtual_diamond_events});
        if (cast_virtual_diamond_events != 0)
            return 53;
        if (cast_diamond_base.events != &cast_virtual_diamond_events)
            return 56;
        if (double_value(3) != 6 || cast_virtual_diamond_events != 0)
            return 54;
    }
    if (cast_virtual_diamond_events != 4321 ||
        lifetime_virtual_diamond_cleanup_events != 4321)
        return 55;
    lifetime_virtual_diamond_cleanup_events = 0;
    int virtual_diamond_events = 0;
    {
        const LifetimeVirtualDiamondBase& diamond_base =
                LifetimeVirtualDiamondDerived{&virtual_diamond_events};
        if (diamond_base.events != &virtual_diamond_events ||
            virtual_diamond_events != 0)
            return 42;
    }
    if (virtual_diamond_events != 4321 ||
        lifetime_virtual_diamond_cleanup_events != 4321)
        return 43;
    lifetime_virtual_diamond_cleanup_events = 0;
    virtual_diamond_events = 0;
    {
        LifetimeVirtualDiamondDerived diamond{&virtual_diamond_events};
        if (lifetime_virtual_diamond_left_events(&diamond) !=
                &virtual_diamond_events ||
            lifetime_virtual_diamond_right_events(&diamond) !=
                &virtual_diamond_events)
            return 44;
    }
    if (virtual_diamond_events != 4321 ||
        lifetime_virtual_diamond_cleanup_events != 4321)
        return 45;
    lifetime_virtual_diamond_cleanup_events = 0;
    virtual_diamond_events = 0;
    {
        LifetimeVirtualDiamondDerived named_diamond{&virtual_diamond_events};
        const LifetimeVirtualDiamondBase&& named_virtual_xvalue =
                static_cast<const LifetimeVirtualDiamondBase&&>(
                    static_cast<LifetimeVirtualDiamondDerived&&>(
                        named_diamond));
        const LifetimeVirtualDiamondBase* named_virtual_pointer =
                &named_virtual_xvalue;
        const LifetimeVirtualDiamondBase* expected_virtual_pointer =
                &named_diamond;
        if (named_virtual_pointer != expected_virtual_pointer)
            return 46;
        if (named_virtual_pointer->events != &virtual_diamond_events)
            return 48;
        if (read_virtual_diamond_xvalue(
                static_cast<LifetimeVirtualDiamondBase&&>(named_diamond)) != 1)
            return 48;
        if (virtual_diamond_events != 0 ||
            lifetime_virtual_diamond_cleanup_events != 0)
            return 49;
    }
    if (virtual_diamond_events != 4321 ||
        lifetime_virtual_diamond_cleanup_events != 4321)
        return 47;
    const int& extended_const_temporary = 47;
    if (read_rvalue(9) != 9) return 14;
    if (extended_const_temporary != 47) return 22;
    if (read_rvalue(mutable_value) != 14) return 15;
    if (invoke(double_value, 6) != 12) return 16;
    if (copy_from_const_pointer(&constant) != 8) return 17;
    if (default_type_copy(13) != 13) return 18;
    if (read_array_element(values) != 6) return 19;
    return 0;
}
