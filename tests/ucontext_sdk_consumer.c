/* SPDX-License-Identifier: MIT */
#include <ucontext.h>

int rcc_ucontext_sdk_capture(ucontext_t* context)
{
    return getcontext(context);
}

int rcc_ucontext_sdk_switch(ucontext_t* current,
                             const ucontext_t* next)
{
    return swapcontext(current, next);
}
