export module rcc_unsupported_module;
import rcc_unsupported_dependency;

int rcc_unsupported_coroutines(void)
{
    co_await 1;
    co_yield 2;
    co_return 3;
}
