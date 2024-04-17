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
#include "stdio.h"
#include "uart.h"

// cmds
#define CMD_DONE '0' // Sent to PC as ack
#define CMD_READ '1' // Read from the EPROM
#define CMD_WRTE '2' // Program the EPROM
#define CMD_CHEK '3' // Check EPROM is blank (all FF))

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
extern void do_blank();

// ****************************************************************************
// main
void main(void) {

    // Initialise uart
    uart_init(115200);
    
    // disable analog else weird things happen
    ADCON0bits.ADON = 0;
    ADCON1 = 0x0F;
    
    // Use port E for status LEDs
    TRISEbits.RE0 = 0; // green LED, while loop
    TRISEbits.RE1 = 0; // red LED, interrupt
    
    // Port A for uart control. Bits 0,1,4-7 spare.
    TRISAbits.RA2 = 1; // CTS is an active low input
    TRISAbits.RA3 = 0; // RTS is an active low output
    PORTAbits.RA3 = 0; // set it true

    // Port D for address bits AD0-A7
    TRISD = 0;
    
    // Port C bits 0,1,2 = A8-A10
    // (uart uses bits 6,7). Bits 3/4/5 spare.
    TRISC = 0b11000000;
    
    // Port B for EPROM control. Bits 4-7 spare.
    TRISB = 0;
    // Port B0 = ALE
    // Port B1 = CE2
    // Port B2 = _RD
    // Port B3 = PGM (switches +25v onto VDD pin)
    PORTBbits.RB3 = 0;
    
    // Loop while waiting for commands
    // We flash a green LED so we know we are listening...
    while (true) {
        PORTEbits.RE0 = 0;
        __delay_ms(250);      
        PORTEbits.RE0 = 1;
        __delay_ms(250);
        
        // If we set this in interrupt, it's because we have
        // 2 chars in the cmd buffer: a $ and cmd id.
        if (cmd_active) {
            
            // Disable global interrupts
            INTCONbits.GIEH = 0;
    
            // Do the cmd
            if (buffer[1] == CMD_DONE) {
                do_finish();
            }
            else if (buffer[1] == CMD_READ) {
                PORTEbits.RE1 = 1;
                do_read();
            }
            else if (buffer[1] == CMD_WRTE) {
                PORTEbits.RE1 = 1;
                do_write();
            }
            else if (buffer[1] == CMD_CHEK) {
                PORTEbits.RE1 = 1;
                do_blank();
            }
            else {
                uart_puts("\nUnknown cmd\n");
            }
            PORTEbits.RE1 = 0;
            bufptr = 0;
            cmd_active = false;
            
            // Enable global interrupts
            INTCONbits.GIEH = 1;
        } 
    } 
}

// ****************************************************************************
// high priority service routine for UART receive
void __interrupt(high_priority) high_isr(void)
{
    char c = 0;
    
    // Disable global interrupts
    INTCONbits.GIEH = 0;
    
    // Echo the character received
    bool ok = uart_getc(&c);
    if (c) {
        // set LED indicating we received a char
        PORTEbits.RE1 = 1;
        
        // ignore CR/LF
        if (c != '\n' && c != '\r') {
            
            // Put received char in buffer  
            buffer[bufptr] = c;

            // Our commands are only 2 chars long
            if (bufptr >= BUFSIZE) {
                bufptr = 0;
                cmd_active=false;
            }
            else {
                if (buffer[0] == '$' && bufptr == 1) {
                    // We have a command
                    cmd_active=true;
                }
            }  
            bufptr++;
        }
        else if (c == 0x03) {
            // ctrl-C
            cmd_active=false;
        }
    }
    
    // Enable global interrupts
    INTCONbits.GIEH = 1;
}

// ****************************************************************************
//
void do_finish()
{
    cmd_active = false;
}

// ****************************************************************************
// check eprom is wiped clean
// Timing critical code. At 20MHz xtal clock, each instruction = 200nS
//
void do_blank()
{
    uint16_t addr;
    char ads[16];
    bool ok = true;
    char *s;
    
    PORTEbits.RE0 = 0; // green led off
    PORTEbits.RE1 = 1; // red led on
        
    // Set CE2 hi to enable reading
    PORTBbits.RB1 = 1;
    // Set PGM lo
    PORTBbits.RB3 = 0;
        
    for (addr = 0; addr < 2048; ++addr) {
        if (cmd_active == false) {
            s = "Check aborted\n";
            uart_puts(s);
            return;
        }

        // Put the address lines out. D0-7 is A0-7, C0-2 is A8-10
        PORTD = addr & 0x00ff;
        PORTCbits.RC0 = addr & 0x0100;
        PORTCbits.RC1 = addr & 0x0200;
        PORTCbits.RC2 = addr & 0x0400;
        
        // Set ALE hi; AD0-7,IO/_M. A8-10, CE2 and _CE1 enter latches
        PORTBbits.RB0 = 1;
        NOP();
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        PORTBbits.RB0 = 0;
        NOP();
        
        // Set port D to input, read it.
        TRISD = 0xff;
        // Set _RD_ lo - 
        PORTBbits.RB2 = 0;
        NOP();
        // Read port D
        uint8_t data = PORTD;

        // Set _RD hi
        PORTBbits.RB2 = 1;

        // Set port D back to output
        TRISD = 0x00;
        
        if (data != 0xff) {
            uart_puts("Blank check fail at address ");
            sprintf(ads, "0x%x = ", addr);
            uart_puts(ads);
            sprintf(ads, "0x%x\n", data);
            uart_puts(ads);
            ok = false;
            break;
        }
    }
    
    // Set CE2 lo to disable reading
    PORTBbits.RB1 = 0;
    
    if (ok) {
        s = "Passed blank check\n";
    } else {
        s = "Failed blank check\n";
    }
    uart_puts(s);
}

// ****************************************************************************
// read from eprom
// Timing critical code. At 20MHz xtal clock, each instruction = 200nS
//
void do_read()
{
    uint16_t addr;
    char ads[16];
    uint8_t col=0;
    char *s;
    
    PORTEbits.RE0 = 0; // green led off
    PORTEbits.RE1 = 1; // red led on
        
    // Set CE2 hi to enable reading
    PORTBbits.RB1 = 1;
    // Set PGM lo
    PORTBbits.RB3 = 0;
        
    for (addr = 0; addr < 2048; ++addr) {
        if (cmd_active == false) {
            s = "Read aborted\n";
            uart_puts(s);
            return;
        }

        // Put the address lines out. D0-7 is A0-7, C0-2 is A8-10
        PORTD = addr & 0x00ff;
        PORTCbits.RC0 = addr & 0x0100;
        PORTCbits.RC1 = addr & 0x0200;
        PORTCbits.RC2 = addr & 0x0400;
        
        // Set ALE hi; AD0-7,IO/_M. A8-10, CE2 and _CE1 enter latches
        PORTBbits.RB0 = 1;
        NOP();
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        PORTBbits.RB0 = 0;
        NOP();
        
        // Set port D to input
        TRISD = 0xff;
        // Set _RD_ lo to enable reading
        PORTBbits.RB2 = 0;
        NOP();
        
        // Read port D - the eprom data.
        uint8_t data = PORTD;

        // Set _RD hi
        PORTBbits.RB2 = 1;

        // Set port D back to output
        TRISD = 0x00;
        
        // Write address
        if (col == 0) {
            sprintf(ads, "%04x: ", addr);
            uart_puts(ads);
        }
        // Write data
        sprintf(ads, "%x", data);
        uart_puts(ads);
        if (col == 15) {
            col = 0;
            uart_putc('\n');
        } else {
            uart_putc(' ');
            col++;
        }
    }
    
    // Set CE2 lo to disable reading
    PORTBbits.RB1 = 0;
    
    s = "CMD_DONE\n";
    uart_puts(s);
}

// ****************************************************************************
// write to eprom
// Timing critical code. At 20MHz xtal clock, each instruction = 200nS
//
void do_write()
{
    uint16_t addr = 0;
    char *s;
    
    PORTEbits.RE0 = 0; // green led off
    PORTEbits.RE1 = 1; // red led on
        
    // Set port D to output
    TRISD = 0x00;
        
    // Set CE2 hi to enable writing
    PORTBbits.RB1 = 1;
    // Set _RD hi
    PORTBbits.RB2 = 1;
    // Set PGM lo
    PORTBbits.RB3 = 0;
        
    while (addr < 2048) {
        if (cmd_active == false) {
            s = "Write aborted\n";
            uart_puts(s);
            return;
        }

        // The sender sends a byte stream.
        char c;
        uart_getc(&c);
        
        // Put the address lines out. D0-7 is A0-7, C0-2 is A8-10
        PORTD = addr & 0x00ff;
        PORTCbits.RC0 = addr & 0x0100;
        PORTCbits.RC1 = addr & 0x0200;
        PORTCbits.RC2 = addr & 0x0400;
        
        // Set ALE hi; AD0-7,IO/_M. A8-10, CE2 and _CE1 enter latches
        PORTBbits.RB0 = 1;
        NOP();NOP();NOP();
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        PORTBbits.RB0 = 0;
        NOP();
        
        // Write the byte to port D
        PORTD = c;
        
        // Activate PGM pulse for 50mS
        __delay_us(10);
        PORTBbits.RB3 = 1;
        __delay_ms(50);
        PORTBbits.RB3 = 0;
        __delay_us(10);
     
        // We should now verify the byte is written,
        // and if necessary try writing again until ok.
        
        // increment address
        addr++;
    }
    
    // Set CE2 lo to disable writing.
    PORTBbits.RB1 = 0;
    
    s = "CMD_DONE\n";
    uart_puts(s);
}





