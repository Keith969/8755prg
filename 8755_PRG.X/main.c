// ****************************************************************************
//
// File                 : main.c
// Hardware Environment : PIC 18F4520
//                        5v supply voltage
//                        internal oscillator
// Build Environment    : MPLAB IDE
// Version              : V8.76
// By                   : Keith Sabine (keith@peardrop.co.uk)
// Created on           : April 10, 2024, 12:47 PM
//
// ****************************************************************************

#include <xc.h>
#include <pic18f4520.h>
#include "conbits.h"
#include "stdint.h"
#include "string.h"
#include "uart.h"

// cmds
#define CMD_DONE '0'
#define CMD_READ '1'
#define CMD_WRIT '2'

// CMD buffer
#define BUFSIZE 2

//
// global variables
//
static uint8_t buffer[BUFSIZE];        // cmd buffer
static uint8_t bufptr;                 // cmd size
static bool    cmd_active = false;     // Complete line received from isr

//
// forward defs
extern void do_finish();
extern void do_read();
extern void do_write();

//
// main
//
void main(void) {

    // Initialise uart
    uart_init(115200);
    
    // Set port A
    TRISAbits.RA0 = 0;
    TRISAbits.RA1 = 0;
    PORTAbits.RA1 = 0; // red LED, RX interrupt
    PORTAbits.RA0 = 0; // green LED, while loop
    // Do not use RA3
    TRISAbits.RA2 = 1; // CTS is an active low input
    TRISAbits.RA4 = 0; // RTS is an active low output
    PORTAbits.RA5 = 0; // set it true

    // Set port B - address 0-7
    TRISB = 0x00;
    // Set port C - address 8-13
    TRISC = 0x3F; 
    // Set port D - data 0-7
    TRISD = 0x00;
    
    // Loop while waiting for interrupts
    // We flash a LED so we know we are listening...
    while (true) {
        PORTAbits.RA0 = 1;
        __delay_ms(100);      
        PORTAbits.RA0 = 0;
        PORTAbits.RA1 = 0;
        __delay_ms(100);
        // If we set this in interrupt, it's because we have
        // 2 chars in the cmd buffer: a $ and cmd id.
        if (cmd_active) {
            // Do the cmd
            if (buffer[1] == CMD_DONE) {
                do_finish();
            }
            else if (buffer[1] == CMD_READ) {
                do_read();
            }
            else if (buffer[1] == CMD_WRIT) {
                do_write();
            }
            bufptr = 0;
            cmd_active = false;
        }
    } 
}

//
// high priority service routine for UART receive
//
void __interrupt(high_priority) high_isr(void)
{
    uint8_t c = 0;
    
    // Disable global interrupts
    INTCONbits.GIEH = 0;
    
    // Echo the character received
    bool ok = uart_getc(&c);
    if (c) {
        // set LED indicating we received a char
        PORTAbits.RA1 = 1;
        
        // Put received char in buffer   
        buffer[bufptr++] = c;

        // If we overshot the buffer size, reset it...
        if (bufptr == BUFSIZE) {
            bufptr = 0;
        }
        else {
            if (buffer[0] == '$' && bufptr == 1) {
                cmd_active=true;
            }
        }    
    }
    
    // Enable global interrupts
    INTCONbits.GIEH = 1;
}

void do_finish()
{
    uint8_t *s = (uint8_t *) "finish\n";
    uart_puts(s);
}

void do_read()
{
    // Set up for reading
}

void do_write()
{
    // Set up for writing
}





