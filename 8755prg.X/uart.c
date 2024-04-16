// ****************************************************************************
//
// File                 : uart.c
// Hardware Environment : PIC 18F4520
//                        5v supply voltage
//                        internal oscillator
// Build Environment    : MPLAB IDE
// Version              : V8.76
// By                   : Keith Sabine (keith@peardrop.co.uk)
// Created on           : April 10, 2024, 12:47 PM
//
// ****************************************************************************

#include "uart.h"
#include <xc.h>

// ****************************************************************************
// Function         [ uart_init ]
// Description      [ ]
// ****************************************************************************
void uart_init(const uint32_t baud_rate)
{
    // disable global interrupts
    INTCONbits.GIEH = 0;
    
    PIE1bits.TX1IE = 1;     // disable TX interrupts
    PIE1bits.RC1IE = 1;     // disable RX interrupts
    
    // Set port C UART bits. Both need to be 1
    TRISCbits.TRISC6=1;     // TX
    TRISCbits.TRISC7=1;     // RX
    
    BAUDCONbits.BRG16 = 1;  // 16-bit Baud Rate Generator
       
    RCSTAbits.CREN = 1; // Continuous receive enable
    RCSTAbits.SPEN = 1; // Serial port enable   
    
    TXSTAbits.SYNC = 0; // async mode
    TXSTAbits.BRGH = 1; // baud rate hi
    TXSTAbits.TXEN = 1; // enable transmit
    
    // Get the constant for baud rate calculation
    uint8_t factor;
    if (BAUDCONbits.BRG16 && TXSTAbits.BRGH)
        factor = 4;
    else if (BAUDCONbits.BRG16 && !TXSTAbits.BRGH)
        factor = 16;
    else if (!BAUDCONbits.BRG16 && TXSTAbits.BRGH)
        factor = 16;
    else if (!BAUDCONbits.BRG16 && !TXSTAbits.BRGH)
        factor = 64;
    if (TXSTAbits.SYNC)
        factor = 64;
    
    // Set baud rate
    uint32_t n = (int32_t) _XTAL_FREQ / (factor * baud_rate) - 1u ;
    SPBRGH = (n & 0xFF00) >> 8;
    SPBRG  =  n & 0x00FF;
    
    RCONbits.IPEN = 1;  // enable priority interrupts
    INTCONbits.PEIE = 1;// enable all unmasked interrupts
    IPR1bits.RCIP = 1;  // receive interrupts high priority
    
    // Enable UART receive interrupt
    PIE1bits.RCIE=1;  
    // Only set TXIE bit if more data to send, then clear it.
    // The TXIF flag is set whenever TXREG is empty.
    PIE1bits.TXIE=0;  
    
    // enable global interrupts
    INTCONbits.GIEH = 1;
}

// ****************************************************************************
// Function         [ uart_putc ]
// Description      [ Send a character ]
// ****************************************************************************
void uart_putc(uint8_t c)
{
    // Disable global interrupts
    INTCONbits.GIEH = 0;
    
    // Set RTS false to prevent sending until TX buffer empty
    PORTAbits.RA3 = 1;
    
    // Wait until TXREG ready
    while (PIR1bits.TXIF == 0) {
        NOP();
    }

    // If CTS is false i.e DTE not ready, wait
    while (PORTAbits.RA2 == 1) {
        NOP();
    }
    
    // Put the char to send in transmit buffer
    TXREG = c;

    // Wait for done
    while(TXSTAbits.TRMT == 0) {
        NOP();
    }
    
    // Set RTS true to allow sending
    PORTAbits.RA3 = 0;
    
    // Enable global interrupts
    INTCONbits.GIEH = 1;
}

// ****************************************************************************
// Function         [ uart_getc ]
// Description      [ Receive a char in c. Returns true if OK ]
// ****************************************************************************
bool uart_getc(uint8_t *c)
{  
    // NULL if we had an error
    bool ok = false;
    
    // Check for errors
    if (RCSTAbits.FERR) {
        uint8_t er = RCREG;  // Framing error
    }
    else if (RCSTAbits.OERR) {
        RCSTAbits.CREN = 0;  // Overrun error, clear it
        RCSTAbits.CREN = 1;
    }
    else {
        *c = RCREG & 0x7f;    // strip hi bit
    } 
    
    return ok;
}

// ****************************************************************************
// Function         [ uart_puts ]
// Description      [ Send a null terminated string ]
// ****************************************************************************
void uart_puts(uint8_t *s)
{
    // Disable global interrupts
    INTCONbits.GIEH = 0;
    
    // Set RTS false to prevent sending until TX buffer empty
    PORTAbits.RA3 = 1;
    
    // Wait until TXREG ready
    while (PIR1bits.TXIF == 0) {
        NOP();
    }
    
    // If CTS is false i.e DTE not ready, wait
    while (PORTAbits.RA2 == 1) {
        NOP();
    }

    // Put the char to send in transmit buffer
    uint8_t *p = s;
    while (*p) {
        TXREG = *p++;
        while(TXSTAbits.TRMT == 0) {
            NOP();
        }
    }

    // Wait for done
    while(TXSTAbits.TRMT == 0) {
        NOP();
    }
    
    // Set RTS true to allow sending
    PORTAbits.RA3 = 0;
    
    // Enable global interrupts
    INTCONbits.GIEH = 1; 
}