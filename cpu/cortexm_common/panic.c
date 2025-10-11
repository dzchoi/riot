/*
 * Copyright (C) 2015 INRIA
 * Copyright (C) 2015 Eistec AB
 * Copyright (C) 2016 OTA keys
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cpu_cortexm_common
 * @{
 *
 * @file
 * @brief       Crash handling functions implementation for ARM Cortex-based MCUs
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @author      Toon Stegen <toon.stegen@altran.com>
 */

#include "cpu.h"
#include "log.h"

#ifdef DEBUG_ASSERT_BREAKPOINT
// Override the weak assert_breakpoint() in core/lib/assert.c.
void assert_breakpoint(void)
{
#   ifdef CoreDebug_DHCSR_C_DEBUGEN_Msk
    // If a debugger is attached, let the debugger break here. Otherwise, we skip it as
    // `bkpt` will cause either a fault escalation to hardfault or a CPU lockup.
    // Note: On Cortex-M0/M0+, CoreDebug->DHCSR will return always 0, unless a debugger
    // has written the magic unlock value (0xA05F0000) to it.
    if ( CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk )
        __asm__("bkpt #1");
#   endif
}
#endif

#ifdef DEVELHELP
static inline void print_ipsr(void)
{
    uint32_t ipsr = __get_IPSR();
    if ( ipsr ) {
        /* if you get here, you might have forgotten to implement the isr
         * for the printed interrupt number */
        LOG_ERROR("Inside ISR/IRQ %d", ((int)ipsr) - 16);
    }
}
#endif

void panic_arch(void)
{
#ifdef DEVELHELP
    print_ipsr();

// This can be done with `CoreDebug->DEMCR |= CoreDebug_DEMCR_VC_HARDERR_Msk` from a
// debugger or from the start of the firmware.
// #ifdef CoreDebug_DHCSR_C_DEBUGEN_Msk
//     if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) {
//         /* if Debug session is running, tell the debugger to break here.
//             Skip it otherwise as this instruction will cause either a fault
//             escalation to hardfault or a CPU lockup */
//         __asm__("bkpt #0");
//     }
// #endif /* CoreDebug_DHCSR_C_DEBUGEN_Msk */
#endif
}
