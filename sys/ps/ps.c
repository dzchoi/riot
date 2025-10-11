/**
 * Print thread information.
 *
 * Copyright (C) 2013, INRIA.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * @ingroup sys_ps
 * @{
 * @file
 * @brief   UNIX like ps command
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @note    The entry 'runtime_usec' in 'MODULE_SCHEDSTATISTICS' is limited
 *          to 2**32 microseconds. So the entry gets reset after ~1.2 hours.
 * @}
 */

#include "irq.h"                // for irq_disable(), irq_restore()
#include "log.h"
#include "thread.h"

#ifdef MODULE_SCHEDSTATISTICS
#include "schedstatistics.h"
#include "ztimer.h"
#endif

#ifdef MODULE_TLSF_MALLOC
#include "tlsf.h"
#include "tlsf-malloc.h"
#endif

#ifdef DEVELHELP
// Returns the hexadecimal value of LR for a non-active thread.
__attribute__((weak)) void thread_get_lr(thread_t* thread, char* buffer)
{
    (void)thread;
    __builtin_strcpy(buffer, "-");
}

// Returns the hexadecimal value of PC for a non-active thread.
__attribute__((weak)) void thread_get_pc(thread_t* thread, char* buffer)
{
    (void)thread;
    __builtin_strcpy(buffer, "-");
}
#endif

/**
 * @brief Prints a list of running threads including stack usage to stdout.
 */
void ps(void)
{
    // If no threads are created yet (running in boot code), display nothing.
    if ( thread_get_active() == NULL )
        return;

    // Disable context switching while taking a snapshot of thread states.
    unsigned state = irq_disable();

    LOG_DEBUG("pid  "                       // "%.3s  ""
#ifdef CONFIG_THREAD_NAMES
       " name              "                // "%c%.16s  "
#endif
        "state     pri  "                   // "%.8s  %.3s  "
#ifdef DEVELHELP
        "lr        pc        stack usage"   // "%.8s  %.8s  %d/%d"
#endif
#ifdef MODULE_SCHEDSTATISTICS
        "  | runtime  | switches  | runtime_usec "
#endif
    );

#if defined(DEVELHELP) && ISR_STACKSIZE
    LOG_DEBUG("  -  "
       " isr_stack         "
        "-           -  "
        "-         -         %d/%d",
        thread_isr_stack_usage(), ISR_STACKSIZE);
#endif

#ifdef MODULE_SCHEDSTATISTICS
    uint64_t rt_sum = 0;
    if (!IS_ACTIVE(MODULE_CORE_IDLE_THREAD)) {
        rt_sum = sched_pidlist[KERNEL_PID_UNDEF].runtime_us;
    }
    for (kernel_pid_t i = KERNEL_PID_FIRST; i <= KERNEL_PID_LAST; i++) {
        thread_t* thread = thread_get(i);
        if (thread != NULL) {
            rt_sum += sched_pidlist[i].runtime_us;
        }
    }
#endif /* MODULE_SCHEDSTATISTICS */

    for ( kernel_pid_t i = KERNEL_PID_FIRST; i <= KERNEL_PID_LAST; i++ ) {
        thread_t* thread = thread_get(i);

        if ( thread != NULL ) {
            const char* state = thread_state_to_string(thread_get_status(thread));
            const char is_active = (thread_get_active() == thread) ? '>' : ' ';
#ifdef DEVELHELP
            int stacksz = thread_get_stacksize(thread);
            char lr[9], pc[9];
            thread_get_lr(thread, lr);
            thread_get_pc(thread, pc);
#endif
#ifdef MODULE_SCHEDSTATISTICS
            /* multiply with 100 for percentage and to avoid floats/doubles */
            uint64_t runtime_us = sched_pidlist[i].runtime_us * 100;
            uint32_t ztimer_us = {sched_pidlist[i].runtime_us};
            unsigned runtime_major = runtime_us / rt_sum;
            unsigned runtime_minor = ((runtime_us % rt_sum) * 1000) / rt_sum;
            unsigned switches = sched_pidlist[i].schedules;
#endif
            LOG_DEBUG("%3d  "
#ifdef CONFIG_THREAD_NAMES
                "%c%-16s  "
#endif
                "%-8s  %3d  "
#ifdef DEVELHELP
                "%-8s  %-8s  %d/%d"
#endif
#ifdef MODULE_SCHEDSTATISTICS
                "  | %2d.%03d%% |  %8u  | %10"PRIu32" "
#endif
                , thread_getpid_of(thread)
#ifdef CONFIG_THREAD_NAMES
                , is_active, thread_get_name(thread)
#endif
                , state, thread_get_priority(thread)
#ifdef DEVELHELP
                , lr, pc
                , stacksz - thread_measure_stack_free(thread), stacksz
#endif
#ifdef MODULE_SCHEDSTATISTICS
                , runtime_major, runtime_minor, switches, ztimer_us
#endif
            );
        }
    }

#ifdef DEVELHELP
#   ifdef MODULE_TLSF_MALLOC
    puts("\nHeap usage:");
    tlsf_size_container_t sizes = { .free = 0, .used = 0 };
    tlsf_walk_pool(tlsf_get_pool(_tlsf_get_global_control()), tlsf_size_walker, &sizes);
    LOG_DEBUG("\tTotal free size: %u", sizes.free);
    LOG_DEBUG("\tTotal used size: %u", sizes.used);
#   endif
#endif

    irq_restore(state);  // Enable context switching.
}
