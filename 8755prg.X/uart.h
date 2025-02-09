// ****************************************************************************
//
// File                 : uart.h
// Hardware Environment : PIC 18F4520
//                        5v supply voltage
//                        internal oscillator
// Build Environment    : MPLAB IDE
// Version              : V8.76
// By                   : Keith Sabine (keith@peardrop.co.uk)
// Created on           : April 10, 2024, 12:47 PM
//
// ****************************************************************************

#ifndef UART_H
#define	UART_H

#include <stdint.h>
#include <stdbool.h>
#include "conbits.h"

#ifdef	__cplusplus
extern "C" {
#endif

// Initialise the UART
void uart_init(const uint32_t baud_rate);

// Set auto baud detection
void uart_set_baudrate();

// Send a char from the UART
void uart_putc(char c);

// Send a string from the UART
void uart_puts(char *s);

// receive a char from the UART
bool  uart_getc(char *c);

// printf like
void uart_printf(const char *fmt, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* UART_H */

