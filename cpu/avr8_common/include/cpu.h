/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *               2014 Freie Universität Berlin, Hinnerk van Bruinehsen
 *               2018 RWTH Aachen, Josua Arndt <jarndt@ias.rwth-aachen.de>
 *               2021 Gerson Fernando Budke <nandojve@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_avr8_common
 * @brief       Common implementations and headers for AVR-8 family based micro-controllers
 * @{
 *
 * @file
 * @brief       Basic definitions for the AVR-8 common module
 *
 * When ever you want to do something hardware related, that is accessing MCUs registers directly,
 * just include this file. It will then make sure that the MCU specific headers are included.
 *
 * @author      Stefan Pfeiffer <stefan.pfeiffer@fu-berlin.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Hinnerk van Bruinehsen <h.v.bruinehsen@fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Josua Arndt <jarndt@ias.rwth-aachen.de>
 * @author      Gerson Fernando Budke <nandojve@gmail.com>
 *
 */

#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdint.h>

#include <avr/interrupt.h>
#include "cpu_conf.h"
#include "cpu_clock.h"
#include "sched.h"
#include "thread.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @name    Use shared I2C functions
 * @{
 */
#define PERIPH_I2C_NEED_READ_REG
#define PERIPH_I2C_NEED_WRITE_REG
#define PERIPH_I2C_NEED_READ_REGS
#define PERIPH_I2C_NEED_WRITE_REGS
/** @} */

/**
 * @name    Flags for the current state of the ATmega MCU
 * @{
 */
#define AVR8_STATE_FLAG_ISR           (0x80U) /**< In ISR */
#define AVR8_STATE_FLAG_UART0_TX      (0x01U) /**< TX pending for UART 0 */
#define AVR8_STATE_FLAG_UART1_TX      (0x02U) /**< TX pending for UART 1 */
#define AVR8_STATE_FLAG_UART2_TX      (0x04U) /**< TX pending for UART 2 */
#define AVR8_STATE_FLAG_UART3_TX      (0x08U) /**< TX pending for UART 3 */
#define AVR8_STATE_FLAG_UART4_TX      (0x10U) /**< TX pending for UART 4 */
#define AVR8_STATE_FLAG_UART5_TX      (0x20U) /**< TX pending for UART 5 */
#define AVR8_STATE_FLAG_UART6_TX      (0x40U) /**< TX pending for UART 6 */
#define AVR8_STATE_FLAG_UART_TX(x)    (0x01U << x) /**< TX pending for UART x */
/** @} */

/**
 * @brief   Global variable containing the current state of the MCU
 *
 * @note    This variable is updated from IRQ context; access to it should
 *          be wrapped into @ref irq_disable and @ref irq_restore or
 *          @ref avr8_get_state should be used.
 *
 * Contents:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+
 * |IRQ|TX6|TX5|TX4|TX3|TX2|TX1|TX0|
 * +---+---+---+---+---+---+---+---+
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * | Label  | Description                                                   |
 * |:-------|:--------------------------------------------------------------|
 * | IRQ    | This bit is set when in IRQ context                           |
 * | TX6    | This bit is set when on UART6 TX is pending                   |
 * | TX5    | This bit is set when on UART5 TX is pending                   |
 * | TX4    | This bit is set when on UART4 TX is pending                   |
 * | TX3    | This bit is set when on UART3 TX is pending                   |
 * | TX2    | This bit is set when on UART2 TX is pending                   |
 * | TX1    | This bit is set when on UART1 TX is pending                   |
 * | TX0    | This bit is set when on UART0 TX is pending                   |
 */
extern uint8_t avr8_state;

/**
 * @brief   Atomically read the state (@ref avr8_state)
 *
 * This function guarantees that the read is not optimized out, not reordered
 * and done atomically. This does not mean that by the time return value is
 * processed that it still reflects the value currently stored in
 * @ref avr8_state.
 *
 * Using ASM rather than C11 atomics has less overhead, as not every access to
 * the state has to be performed atomically: Those done from ISR will not be
 * interrupted (no support for nested interrupts) and barriers at the begin and
 * end of the ISRs make sure the access takes place before IRQ context is left.
 */
static inline uint8_t avr8_get_state(void)
{
    uint8_t state;
    __asm__ volatile(
        "lds   %[state], avr8_state       \n\t"
        : [state]   "=r" (state)
        :
        : "memory"

    );

    return state;
}

/**
 * @brief   Run this code on entering interrupt routines
 */
static inline void avr8_enter_isr(void)
{
    /* This flag is only called from IRQ context, and nested IRQs are not
     * supported as of now. The flag will be unset before the IRQ context is
     * left, so no need to use memory barriers or atomics here
     */
    avr8_state |= AVR8_STATE_FLAG_ISR;
}

/**
 * @brief   Check if TX on any present UART device is still pending
 *
 * @retval  !=0     At least on UART device is still sending data out
 * @retval  0       No UART is currently sending data
 */
static inline int avr8_is_uart_tx_pending(void)
{
    uint8_t state = avr8_get_state();
    return (state & (AVR8_STATE_FLAG_UART0_TX
                  |  AVR8_STATE_FLAG_UART1_TX
                  |  AVR8_STATE_FLAG_UART2_TX
                  |  AVR8_STATE_FLAG_UART3_TX
                  |  AVR8_STATE_FLAG_UART4_TX
                  |  AVR8_STATE_FLAG_UART5_TX
                  |  AVR8_STATE_FLAG_UART6_TX));
}

/**
 * @brief Run this code on exiting interrupt routines
 */
void avr8_exit_isr(void);

/**
 * @brief Initialization of the CPU clock
 */
void avr8_clk_init(void);

/**
 * @brief   Print the last instruction's address
 *
 * @details This works only if called in a function as first statement, as
 *          it relies on the return address to be the topmost item on the stack.
 */
static inline void __attribute__((always_inline)) cpu_print_last_instruction(void)
{
        uint32_t addr;
    __asm__ volatile(
        "ldi %D[dest], 0"                   "\n\t"
#if __AVR_3_BYTE_PC__
        "pop %C[dest] "      "\n\t"
#else
        "ldi %C[dest], 0"                   "\n\t"
#endif
        "pop %B[dest]"                      "\n\t"
        "pop %A[dest]"                      "\n\t"
        "push %A[dest]"                     "\n\t"
        "push %B[dest]"                     "\n\t"
#if __AVR_3_BYTE_PC__
        "push %C[dest] "      "\n\t"
#endif
        : [dest]    "=r"(addr)
        : /* no inputs */
        : "memory"
    );

    /* addr now contains instruction to return to, subtract one to get
     * the instruction that called this function. Also multiply by two to get
     * the byte position, rather than the (16 bit) instruction position */
    addr = (addr - 1 ) * 2;
    printf("0x%" PRIx32 "\n", addr);
}

/**
 * @brief   Initializes avrlibc stdio
 */
void avr8_stdio_init(void);

/**
 * @brief   Print reset cause
 */
void avr8_reset_cause(void);

#ifdef __cplusplus
}
#endif

#endif /* CPU_H */
/** @} */
