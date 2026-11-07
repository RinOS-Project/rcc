namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};
}

extern "C" void* rin_coro_handle_from_promise(void* promise);

extern "C" {
int rcc_coroutine_exception_unhandled_count;
int rcc_coroutine_exception_caught_value;
int rcc_coroutine_exception_local_destroy_count;
int rcc_coroutine_exception_payload_destroy_count;
int rcc_coroutine_exception_protocol_stage;
int rcc_coroutine_exception_awaiter_destroy_count;
void* rcc_coroutine_exception_last_frame;
}

struct rcc_coroutine_exception_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_exception_suspending_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_exception_throwing_protocol_awaiter {
    ~rcc_coroutine_exception_throwing_protocol_awaiter() noexcept {
        ++rcc_coroutine_exception_awaiter_destroy_count;
    }
    bool await_ready() const {
        if (rcc_coroutine_exception_protocol_stage == 1) throw 61;
        return rcc_coroutine_exception_protocol_stage != 2;
    }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const {
        if (rcc_coroutine_exception_protocol_stage == 2) throw 62;
    }
    void await_resume() const {
        if (rcc_coroutine_exception_protocol_stage == 3) throw 63;
    }
};

struct rcc_coroutine_exception_promise {
    void get_return_object() noexcept {
        rcc_coroutine_exception_last_frame =
            rin_coro_handle_from_promise(this);
    }
    rcc_coroutine_exception_ready_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_exception_ready_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {
        ++rcc_coroutine_exception_unhandled_count;
    }
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_exception_promise;
};
}

struct rcc_coroutine_exception_tracked_local {
    ~rcc_coroutine_exception_tracked_local() noexcept {
        ++rcc_coroutine_exception_local_destroy_count;
    }
};

struct rcc_coroutine_exception_payload {
    int value;
    ~rcc_coroutine_exception_payload() noexcept {
        ++rcc_coroutine_exception_payload_destroy_count;
    }
};

struct rcc_coroutine_exception_throwing_initializer_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_exception_throwing_constructor_awaiter {
    rcc_coroutine_exception_throwing_constructor_awaiter() {
        throw 79;
    }
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

int rcc_coroutine_throwing_initializer() {
    throw 17;
}

rcc_coroutine_exception_throwing_initializer_awaiter
rcc_coroutine_exception_make_awaiter() {
    throw 71;
}

void rcc_coroutine_body_throw() {
    throw 41;
    co_return;
}

void rcc_coroutine_initializer_throw() {
    int value = rcc_coroutine_throwing_initializer();
    (void)value;
    co_return;
}

void rcc_coroutine_caught_throw() {
    try {
        throw 7;
    } catch (int value) {
        rcc_coroutine_exception_caught_value = value;
        try {
            throw;
        } catch (int rethrown_value) {
            rcc_coroutine_exception_caught_value = rethrown_value + 1;
        }
    }
    co_return;
}

void rcc_coroutine_throw_after_suspend() {
    rcc_coroutine_exception_tracked_local local{};
    co_await rcc_coroutine_exception_suspending_awaiter{};
    throw 29;
    co_return;
}

void rcc_coroutine_throw_class_after_suspend() {
    co_await rcc_coroutine_exception_suspending_awaiter{};
    throw rcc_coroutine_exception_payload{53};
    co_return;
}

void rcc_coroutine_protocol_throw() {
    co_await rcc_coroutine_exception_throwing_protocol_awaiter{};
    co_return;
}

void rcc_coroutine_throwing_awaiter_initializer() {
    co_await rcc_coroutine_exception_make_awaiter();
    co_return;
}

void rcc_coroutine_throwing_awaiter_constructor() {
    co_await rcc_coroutine_exception_throwing_constructor_awaiter{};
    co_return;
}
