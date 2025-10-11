/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cortexm_common
 * @{
 *
 * @file
 * @brief       Default implementations for Cortex-M specific interrupt and
 *              exception handlers
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Daniel Krebs <github@daniel-krebs.net>
 * @author      Joakim Gebart <joakim.gebart@eistec.se>
 * @author      Sören Tempel <tempel@uni-bremen.de>
 *
 * @}
 */

#include <stdint.h>
#include <inttypes.h>

#include "cpu.h"
#include "periph_cpu.h"
#include "kernel_init.h"
#include "board.h"
#include "log.h"
#include "mpu.h"
#include "panic.h"
#include "sched.h"
#include "vectors_cortexm.h"
#ifdef MODULE_PUF_SRAM
#include "puf_sram.h"
#endif
#ifdef MODULE_DBGPIN
#include "dbgpin.h"
#endif

#ifndef SRAM_BASE
#define SRAM_BASE 0
#endif

#ifndef CPU_BACKUP_RAM_NOT_RETAINED
#define CPU_BACKUP_RAM_NOT_RETAINED 0
#endif

/**
 * @brief   Memory markers, defined in the linker script
 * @{
 */
extern uint32_t _sfixed;
extern uint32_t _efixed;
extern uint32_t _etext;
extern uint32_t _srelocate;
extern uint32_t _erelocate;
extern uint32_t _szero;
extern uint32_t _ezero;
extern uint32_t _sstack;
extern uint32_t _estack;
extern uint8_t _sram;
extern uint8_t _eram;

/* Support for LPRAM. */
#ifdef CPU_HAS_BACKUP_RAM
extern const uint32_t _sbackup_data_load[];
extern uint32_t _sbackup_data[];
extern uint32_t _ebackup_data[];
extern uint32_t _sbackup_bss[];
extern uint32_t _ebackup_bss[];
#endif /* CPU_HAS_BACKUP_RAM */
/** @} */

/**
 * @brief   Allocation of the interrupt stack
 */
__attribute__((used, section(".isr_stack"), aligned(8)))
uint8_t isr_stack[ISR_STACKSIZE];

/**
 * @brief   Pre-start routine for CPU-specific settings
 */
__attribute__((weak)) void pre_startup (void)
{
}

/**
 * @brief   Post-start routine for CPU-specific settings
 */
__attribute__((weak)) void post_startup (void)
{
}

void reset_handler_default(void)
{
    uint32_t *dst;
    const uint32_t *src = &_etext;

#ifdef __ARM_ARCH_8M_MAIN__
    /* Set the lower limit of the exception stack into MSPLIM register */
    __set_MSPLIM((uint32_t)&_sstack);
#endif

    cortexm_init_fpu();

#ifdef MODULE_PUF_SRAM
    puf_sram_init((uint8_t *)&_srelocate, SEED_RAM_LEN);
#endif

    pre_startup();

#ifdef DEVELHELP
    /* cppcheck-suppress constVariable
     * (top is modified by asm) */
    uint32_t *top;
    /* Fill stack space with canary values up until the current stack pointer */
    /* Read current stack pointer from CPU register */
    __asm__ volatile ("mov %[top], sp" : [top] "=r" (top) : : );
    dst = &_sstack;
    while (dst < top) {
        *(dst++) = STACK_CANARY_WORD;
    }
#endif

    /* load data section from flash to ram */
    /* cppcheck-suppress comparePointers
     * (addresses exported as symbols via linker script and look unrelated
     * to cppcheck) */
    for (dst = &_srelocate; dst < &_erelocate; ) {
        *(dst++) = *(src++);
    }

    /* default bss section to zero */
    /* cppcheck-suppress comparePointers
     * (addresses exported as symbols via linker script and look unrelated
     * to cppcheck) */
    for (dst = &_szero; dst < &_ezero; ) {
        *(dst++) = 0;
    }

#ifdef CPU_HAS_BACKUP_RAM
#if BACKUP_RAM_HAS_INIT
    backup_ram_init();
#endif
    if (!cpu_woke_from_backup() ||
        CPU_BACKUP_RAM_NOT_RETAINED) {

        /* load low-power data section. */
        for (dst = _sbackup_data, src = _sbackup_data_load;
             dst < _ebackup_data;
             dst++, src++) {
            *dst = *src;
        }

        /* zero-out low-power bss. */
        /* cppcheck-suppress comparePointers
         * (addresses exported as symbols via linker script and look unrelated
         * to cppcheck) */
        for (dst = _sbackup_bss; dst < _ebackup_bss; dst++) {
            *dst = 0;
        }
    }
#endif /* CPU_HAS_BACKUP_RAM */

#ifdef MODULE_MPU_NOEXEC_RAM
    /* This marks the memory region from 0x20000000 to 0x3FFFFFFF as non
     * executable. This is the Cortex-M SRAM region used for on-chip RAM.
     */
    mpu_configure(
        0,                                               /* Region 0 (lowest priority) */
        (uintptr_t)&_sram,                               /* RAM base address */
        MPU_ATTR(1, AP_RW_RW, 0, 1, 0, 1, MPU_SIZE_512M) /* Allow read/write but no exec */
    );
#endif

#ifdef MODULE_MPU_STACK_GUARD
    if (((uintptr_t)&_sstack) != SRAM_BASE) {
        mpu_configure(
            1,                                              /* MPU region 1 */
            (uintptr_t)&_sstack + 31,                       /* Base Address (rounded up) */
            MPU_ATTR(1, AP_RO_RO, 0, 1, 0, 1, MPU_SIZE_32B) /* Attributes and Size */
        );

    }
#endif

#if defined(MODULE_MPU_STACK_GUARD) || defined(MODULE_MPU_NOEXEC_RAM)
    mpu_enable();
#endif

    post_startup();

#ifdef MODULE_DBGPIN
    dbgpin_init();
#endif

    /* initialize the CPU */
    extern void cpu_init(void);
    cpu_init();

    /* initialize the board (which also initiates CPU initialization) */
    board_init();

#if MODULE_NEWLIB || MODULE_PICOLIBC
    /* initialize std-c library (this must be done after board_init) */
    extern void __libc_init_array(void);
    __libc_init_array();
#endif

    /* startup the kernel */
    kernel_init();
}

__attribute__((weak))
void nmi_handler(void)
{
    core_panic(PANIC_NMI_HANDLER, "NMI HANDLER");
}

#ifdef DEVELHELP

void common_fault_handler(uint32_t* sp, uint32_t exc_return, uint32_t* r4_to_r11);

// Trampoline function to save stack pointer before calling the common fault handler.
// Note that unlike the original implementation, this handler does not check for MSP
// overflow or reset it to _sstack. If MSP is corrupted while stacking r4–r11 or during
// LOG_ERROR(), a nested fault or system lockup may occur.
__attribute__((naked)) void common_fault_default(void)
{
    __asm__ volatile
    (
        // Get active stack pointer where exception stack frame lies.
        "mov r0, sp                         \n" /* r0 = msp                   */
        "tst lr, #4                         \n" /*                            */
        "beq 1f                             \n" /* if (lr & 0x4 != 0)         */
        "mrs r0, psp                        \n" /*   r0 = psp                 */
        "1:\n"

#if (defined(CPU_CORE_CORTEX_M0) || defined(CPU_CORE_CORTEX_M0PLUS)) \
    && defined(MODULE_CPU_CHECK_ADDRESS)
        // Catch intended HardFaults on Cortex-M0 to probe memory addresses.
        "ldr     r1, [r0, #0x04]            \n" /* read R1 from the stack        */
        "ldr     r2, =0xDEADF00D            \n" /* magic number to be found      */
        "cmp     r1, r2                     \n" /* compare with the magic number */
        "bne     regular_handler            \n" /* no magic -> handle as usual   */
        "ldr     r1, [r0, #0x08]            \n" /* read R2 from the stack        */
        "ldr     r2, =0xCAFEBABE            \n" /* 2nd magic number to be found  */
        "cmp     r1, r2                     \n" /* compare with 2nd magic number */
        "bne     regular_handler            \n" /* no magic -> handle as usual   */
        "ldr     r1, [r0, #0x18]            \n" /* read PC from the stack        */
        "adds    r1, r1, #2                 \n" /* move to the next instruction  */
        "str     r1, [r0, #0x18]            \n" /* modify PC in the stack        */
        "ldr     r5, =0                     \n" /* set R5 to indicate HardFault  */
        "bx      lr                         \n" /* exit the exception handler    */
        " regular_handler:                  \n"
#endif

#if defined(CPU_CORE_CORTEX_M0) || defined(CPU_CORE_CORTEX_M0PLUS) \
    || defined(CPU_CORE_CORTEX_M23)
        "mov r12, r0                        \n" /* save original r0           */
        "mov r0, r8                         \n"
        "mov r1, r9                         \n"
        "mov r2, r10                        \n"
        "mov r3, r11                        \n"
        "push {r0-r3}                       \n" /* save r8-r11 to MSP stack   */
        "push {r4-r7}                       \n" /* save r4-r7 to MSP stack    */
        "mov r0, r12                        \n" /* restore original r0        */
#else
        "push {r4-r11}                      \n" /* save r4..r11 to MSP stack  */
#endif

        // Set r1 and r2.
        "mov r1, lr                         \n" /* r1 = `exc_return` param    */
        "mov r2, sp                         \n" /* r2 = `r4_to_r11` param     */
        "bl common_fault_handler            \n"
    );
}

#if defined(CPU_CORE_CORTEX_M0) || defined(CPU_CORE_CORTEX_M0PLUS) \
    || defined(CPU_CORE_CORTEX_M23)
/* Cortex-M0, Cortex-M0+ and Cortex-M23 lack the extended fault status
   registers found in Cortex-M3 and above. */
#define CPU_HAS_EXTENDED_FAULT_REGISTERS 0
#else
#define CPU_HAS_EXTENDED_FAULT_REGISTERS 1
#endif

static int hex_width(uint32_t val)
{
    return val >= 0x10000 ? 8 :
           val >= 0x100 ? 4 :
           val >= 10 ? 2 : 1;
}

static void log_register(const char* regname, uint32_t regval)
{
    const int n = hex_width(regval);
    LOG_ERROR("%-5s %*s%0*X", regname, 8 - n, "", n, regval);
}

static void log_two_registers(
    const char* regname1, uint32_t regval1, const char* regname2, uint32_t regval2)
{
    const int n1 = hex_width(regval1);
    const int n2 = hex_width(regval2);

    // T32-style register view with compact hex formatting.
    LOG_ERROR("%-5s %*s%0*X  %-5s %*s%0*X",
        regname1, 8 - n1, "", n1, regval1,
        regname2, 8 - n2, "", n2, regval2);
}

__attribute__((used))
void common_fault_handler(uint32_t* sp, uint32_t exc_return, uint32_t* r4_to_r11)
{
#if CPU_HAS_EXTENDED_FAULT_REGISTERS
    static const uint32_t BFARVALID_MASK = (0x80 << SCB_CFSR_BUSFAULTSR_Pos);
    static const uint32_t MMARVALID_MASK = (0x80 << SCB_CFSR_MEMFAULTSR_Pos);

    // Copy fault status registers to local variables before calling any other functions,
    // to avoid corrupting the original contents.
    uint32_t bfar  = SCB->BFAR;
    uint32_t mmfar = SCB->MMFAR;
    uint32_t cfsr  = SCB->CFSR;
    uint32_t hfsr  = SCB->HFSR;
    uint32_t dfsr  = SCB->DFSR;
    uint32_t afsr  = SCB->AFSR;
#endif

    // Check if the ISR stack (MSP) overflowed prior to this point.
    if ( *(&_sstack) != STACK_CANARY_WORD )
        LOG_ERROR("ISR stack overflowed");

    // `sp` points to the active stack pointer prior to the fault, where the Exception
    // Stack Frame (ESF) has been saved.
    // uint32_t   R0 = sp[0];
    // uint32_t   R1 = sp[1];
    // uint32_t   R2 = sp[2];
    // uint32_t   R3 = sp[3];
    // uint32_t  R12 = sp[4];
    // uint32_t   LR = sp[5];  // Link register
    // uint32_t   PC = sp[6];  // Program counter
    // uint32_t xPSR = sp[7];  // Program status register
    // sp[8-25] could hold the FPU registers if `exc_return` & 0x10 == 0.
    // sp[8 or 26] could hold a 4-byte padding if `xPSR` & (1 << 9) != 0.

    // Calculate original SP below the ESF.
    uint32_t* orig_sp =
        sp + 8 + (18 * ((exc_return & 0x10) == 0)) + ((sp[7] & (1u << 9)) != 0);

    // Fault number: HardFault = 3, MemManage = 4, BusFault = 5, UsageFault = 6.
    unsigned ipsr = __get_IPSR();

    // Check whether the fault occurred in Thread mode or Handler mode.
    if ( exc_return & 0x08 ) {  // Thread mode
        // Note that faulting from boot code (before any threads are created) will
        // return pid = 0 here.
        kernel_pid_t pid = thread_getpid();
        LOG_ERROR("\nFault [%d] occurred in thread %"PRIi16" (\"%s\")",
            ipsr, pid, thread_getname(pid));
    }
    else  // Handler mode
        // Note that `sp[7] & 0xff` gives the context in which the code was running:
        //   - 0     -> running in Thread mode
        //   - 1-15  -> running in an ISR handler (Reset, NMI, HardFault, etc.)
        //   - >= 16 -> running in an IRQ (IRQ0 = 16, IRQ1 = 17, ...)
        LOG_ERROR("\nFault [%d] occurred in ISR/IRQ %d",
            ipsr, (int)(sp[7] & 0x1ff) - 16);

    log_two_registers("R0", sp[0], "R7", r4_to_r11[3]);
    log_two_registers("R1", sp[1], "R8", r4_to_r11[4]);
    log_two_registers("R2", sp[2], "R9", r4_to_r11[5]);
    log_two_registers("R3", sp[3], "R10", r4_to_r11[6]);
    log_two_registers("R4", r4_to_r11[0], "R11", r4_to_r11[7]);
    log_two_registers("R5", r4_to_r11[1], "R12", sp[4]);
    log_two_registers("R6", r4_to_r11[2], "SP", (uintptr_t)orig_sp);
    log_two_registers("LR", sp[5], "PC", sp[6]);
    log_register("xPSR", sp[7]);

#if CPU_HAS_EXTENDED_FAULT_REGISTERS
    log_register("CFSR", cfsr);
    log_register("HFSR", hfsr);
    log_register("DFSR", dfsr);
    log_register("AFSR", afsr);

    if ( cfsr & BFARVALID_MASK )
        log_register("BFAR", bfar);

    if ( cfsr & MMARVALID_MASK )
        log_register("MMFAR", mmfar);
#endif

    log_register("EXC_RETURN", exc_return);

    switch ( ipsr ) {
        case 3:
            core_panic(PANIC_HARD_FAULT, "HARD FAULT HANDLER");
            break;

        case 4:
            core_panic(PANIC_MEM_MANAGE, "MEM MANAGE HANDLER");
            break;

        case 5:
            core_panic(PANIC_BUS_FAULT, "BUS FAULT HANDLER");
            break;

        case 6:
            core_panic(PANIC_USAGE_FAULT, "USAGE FAULT HANDLER");
            break;

        default:
            core_panic(PANIC_UNDEFINED, "PANIC_UNDEFINED");
            break;
    }
}

#else

void hard_fault_default(void)
{
    core_panic(PANIC_HARD_FAULT, "HARD FAULT HANDLER");
}

#endif /* DEVELHELP */

#if defined(CPU_CORE_CORTEX_M3) || defined(CPU_CORE_CORTEX_M33) || \
    defined(CPU_CORE_CORTEX_M4) || defined(CPU_CORE_CORTEX_M4F) || \
    defined(CPU_CORE_CORTEX_M7)
# ifdef DEVELHELP
// These handlers will fall back to `common_fault_default()` unless overridden.
__attribute__((weak, alias("common_fault_default"))) void hard_fault_default(void);
__attribute__((weak, alias("common_fault_default"))) void mem_manage_default(void);
__attribute__((weak, alias("common_fault_default"))) void bus_fault_default(void);
__attribute__((weak, alias("common_fault_default"))) void usage_fault_default(void);
# else
void mem_manage_default(void)
{
    core_panic(PANIC_MEM_MANAGE, "MEM MANAGE HANDLER");
}

void bus_fault_default(void)
{
    core_panic(PANIC_BUS_FAULT, "BUS FAULT HANDLER");
}

void usage_fault_default(void)
{
    core_panic(PANIC_USAGE_FAULT, "USAGE FAULT HANDLER");
}
# endif  // DEVELHELP

void debug_mon_default(void)
{
    core_panic(PANIC_DEBUG_MON, "DEBUG MON HANDLER");
}
#endif

void dummy_handler_default(void)
{
    core_panic(PANIC_DUMMY_HANDLER, "DUMMY HANDLER");
}

/* Cortex-M common interrupt vectors */
__attribute__((weak, alias("dummy_handler_default"))) void isr_svc(void);
__attribute__((weak, alias("dummy_handler_default"))) void isr_pendsv(void);
__attribute__((weak, alias("dummy_handler_default"))) void isr_systick(void);

/* define Cortex-M base interrupt vectors
 * IRQ entries -9 to -6 inclusive (offsets 0x1c to 0x2c of cortexm_base_t)
 * are reserved entries. */
ISR_VECTOR(0) const cortexm_base_t cortex_vector_base = {
    &_estack,
    {
        /* entry point of the program */
        [ 0] = reset_handler_default,
        /* [-14] non maskable interrupt handler */
        [ 1] = nmi_handler,
        /* [-13] hard fault exception */
        [ 2] = hard_fault_default,
        /* [-5] SW interrupt, in RIOT used for triggering context switches */
        [10] = isr_svc,
        /* [-2] pendSV interrupt, in RIOT use to do the actual context switch */
        [13] = isr_pendsv,
        /* [-1] SysTick interrupt, not used in RIOT */
        [14] = isr_systick,

        /* -9 to -6 reserved entries can be defined by the cpu module */
        #ifdef CORTEXM_VECTOR_RESERVED_0X1C
        [6] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X1C),
        #endif  /* CORTEXM_VECTOR_RESERVED_0X1C */
        #ifdef CORTEXM_VECTOR_RESERVED_0X20
        [7] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X20),
        #endif  /* CORTEXM_VECTOR_RESERVED_0X20 */
        #ifdef CORTEXM_VECTOR_RESERVED_0X24
        [8] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X24),
        #endif  /* CORTEXM_VECTOR_RESERVED_0X24 */
        #ifdef CORTEXM_VECTOR_RESERVED_0X28
        [9] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X28),
        #endif  /* CORTEXM_VECTOR_RESERVED_0X28 */

        /* additional vectors used by M3, M33, M4(F), and M7 */
#if defined(CPU_CORE_CORTEX_M3) || defined(CPU_CORE_CORTEX_M33) || \
    defined(CPU_CORE_CORTEX_M4) || defined(CPU_CORE_CORTEX_M4F) || \
    defined(CPU_CORE_CORTEX_M7)
        /* [-12] memory manage exception */
        [ 3] = mem_manage_default,
        /* [-11] bus fault exception */
        [ 4] = bus_fault_default,
        /* [-10] usage fault exception */
        [ 5] = usage_fault_default,
        /* [-4] debug monitor exception */
        [11] = debug_mon_default,
#endif
    }
};
