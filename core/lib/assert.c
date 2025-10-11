/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#include "architecture.h"
#include "assert.h"
#include "cpu.h"
#include "debug.h"
#include "irq.h"
#include "log.h"
#include "panic.h"
#if IS_USED(MODULE_BACKTRACE)
#include "backtrace.h"
#endif

#ifdef DEBUG_ASSERT_BREAKPOINT
__attribute__((weak)) void assert_breakpoint(void)
{
    DEBUG_BREAKPOINT(1);
}
#endif

__NORETURN static inline void _assert_common(void)
{
#if IS_USED(MODULE_BACKTRACE)
    LOG_ERROR("Backtrace:");
    backtrace_print();
#endif
#if DEBUG_ASSERT_NO_PANIC
    if (!irq_is_in() && irq_is_enabled()) {
        while (1) {
            thread_sleep();
        }
    }
#endif

    core_panic(PANIC_ASSERT_FAIL, "FAILED ASSERTION");
}

__NORETURN void _assert_failure(const char *file, unsigned line)
{
#ifdef DEBUG_ASSERT_BREAKPOINT
    assert_breakpoint();
#endif
    LOG_ERROR("%s:%u => FAILED ASSERTION", file, line);
    _assert_common();
}

__NORETURN void _assert_panic(void)
{
#ifdef DEBUG_ASSERT_BREAKPOINT
    assert_breakpoint();
#endif
    LOG_ERROR("0x%"PRIxTXTPTR" => FAILED ASSERTION", cpu_get_caller_pc());
    _assert_common();
}

/** @} */
