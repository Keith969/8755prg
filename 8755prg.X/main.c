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
#define CMD_DONE '0'               // Sent to PC as ack
#define CMD_READ '1'               // Read from the EPROM
#define CMD_WRTE '2'               // Program the EPROM
#define CMD_CHEK '3'               // Check EPROM is blank (all FF))

// Stack
#define STACKSIZE 1024             // stack size
#define TOP       STACKSIZE-1      // top of stack
#define BOTTOM    0                // bottom of stack
#define LOWATER   32               // The stack low water mark.

//
// static variables
//
static char    stack[STACKSIZE];   // The receiver stack
static int16_t sptr = TOP;         // The stack pointer
static bool    cmd_active = false; // Are we in a cmd?
static int16_t bytes_pushed = 0;   // received
static int16_t bytes_popped = 0;   // used


// ****************************************************************************
// setCTS()
// Note CTS is active low. So setCTS(1) means 'NOT clear to send'
//
void setCTS(bool b)
{
    PORTAbits.RA2 = b;
}

// ****************************************************************************
// reset the stack
//
void clear()
{
    memset(stack, 0x00, STACKSIZE);
    sptr         = TOP;
    cmd_active   = false;
}

// ****************************************************************************
// push a char onto stack. 
// If there are less than LOWATER chars space left on stack,
// set CTS inactive. Else set CTS active.
//
void push(char c)
{
    if (sptr >= 0) {
        stack[sptr--] = c;

        if (sptr < LOWATER) {
            setCTS(true);
        }
        else {
            setCTS(false);
        }
        bytes_pushed++;
    } 
    else {
        while (1) {
            // Error, light red led
            PORTEbits.RE2 = 1;
        }
    }
}

// ****************************************************************************
// pop a char from stack. 
// If there are less than LOWATER chars space left on stack,
// set CTS inactive. Else set CTS active.
//
char pop()
{
    if (sptr <= TOP) {
        char c = stack[++sptr];
        if (sptr < LOWATER) {
            setCTS(true);
        }
        else {
            setCTS(false);
        }
        bytes_popped++;
        return c;
    }
    else {
        while (1) {
            // Error, light red led
            PORTEbits.RE2 = 1;
        }
    }
}

// ****************************************************************************
// get the cmd at top of stack 
//
char top()
{
    return stack[TOP-1];
}

// ****************************************************************************
// convert char to hex digit
//
uint8_t charToHexDigit(char c)
{
  if (c >= 'a')
    return c - 'a' + 10;
  else if (c >= 'A')
    return c - 'A' + 10;
  else
    return c - '0';
}

// ****************************************************************************
// Initialise the ports
//
void ports_init(void)
{
    // disable analog else weird things happen
    ADCON0bits.ADON = 0;
    ADCON1 = 0x0F;
    
    // Use port E for status LEDs
    TRISEbits.RE0 = 0; // green LED, while loop
    TRISEbits.RE1 = 0; // orange LED, interrupt
    TRISEbits.RE2 = 0; // red LED, warning
    PORTEbits.RE0 = 0; // set all off
    PORTEbits.RE1 = 0;
    PORTEbits.RE2 = 0;
    
    // Port A for uart control. Bits 0,1,4-7 spare.
    TRISAbits.RA2 = 0; // CTS is an active low output
    TRISAbits.RA3 = 1; // RTS is an active low input
    PORTAbits.RA2 = 0; // assert CTS

    // Port D for address bits AD0-A7
    TRISD = 0;
    
    // Port C bits 0,1,2 = A8-A10
    // (uart uses bits 6,7). Bits 3/4/5 spare.
    TRISC = 0b11000000;
    
    // Port B for EPROM control. Bits 5-7 spare.
    TRISB = 0;
    // Port B0 = ALE
    // Port B1 = CE2
    // Port B2 = _RD
    // Port B3 = PGM (switches +25v onto VDD pin)
    // Port B4 = _CE1 (set hi for PGM))
    PORTBbits.RB3 = 0;
    PORTBbits.RB4 = 0;
}

// ****************************************************************************
// high priority service routine for UART receive
//
void __interrupt(high_priority) high_isr(void)
{
    char c = 0;
    
    // Disable interrupts
    INTCONbits.GIEH = 0;
    PIE1bits.RCIE=0;
    
    // Get the character from uart
    bool ok = uart_getc(&c);
    if (ok) {
        // Push the char onto stack
        push(c);

        // Check if we have a cmd yet
        if (stack[TOP] == '$' && (sptr < TOP)) {
            // We have a command (2 chars at top of SP))
            cmd_active = true;
        }
    }
    
    // Enable interrupts
    PIE1bits.RCIE=1;
    INTCONbits.GIEH = 1;
}

// ****************************************************************************
//
//
void do_finish()
{
    clear();
}

// ****************************************************************************
// check eprom is wiped clean
// Timing critical code. At 20MHz xtal clock, each instruction = 200nS
//
void do_blank()
{
    uint16_t addr;
    char ads[32];
    bool ok = true;
    char *s;
        
    // Set CE2 hi to enable reading
    PORTBbits.RB1 = 1;
    // Set PGM lo
    PORTBbits.RB3 = 0;
    // Set _CE1 lo
    PORTBbits.RB4 = 0;
        
    for (addr = 0; addr < 2048; ++addr) {
        if (cmd_active == false) {
            s = "Check aborted\n";
            uart_puts(s);
            return;
        }

        // Put the address lines out. D0-7 is A0-7, C0-2 is A8-10
        PORTD = addr & 0x00ff;
        PORTCbits.RC0 = (addr >>  8) & 0x01;
        PORTCbits.RC1 = (addr >>  9) & 0x01;
        PORTCbits.RC2 = (addr >> 10) & 0x01;
        
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
        uart_puts(s);
    }  
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
        
    // Set CE2 hi to enable reading
    PORTBbits.RB1 = 1;
    // Set PGM lo
    PORTBbits.RB3 = 0;
    // Set _CE1 lo
    PORTBbits.RB4 = 0;
        
    for (addr = 0; addr < 2048; ++addr) {
        if (cmd_active == false) {
            char *s = "Read aborted\n";
            uart_puts(s);
            return;
        }

        // Put the address lines out. D0-7 is A0-7, C0-2 is A8-10
        PORTD = addr & 0x00ff;
        PORTCbits.RC0 = (addr >>  8) & 0x01;
        PORTCbits.RC1 = (addr >>  9) & 0x01;
        PORTCbits.RC2 = (addr >> 10) & 0x01;
        
        // Set ALE hi; AD0-7,IO/_M. A8-10, CE2 and _CE1 enter latches
        PORTBbits.RB0 = 1;
        __delay_us(1);
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        PORTBbits.RB0 = 0;
        __delay_us(1);
        
        // Set port D to input
        TRISD = 0xff;
        // Set _RD_ lo to enable reading
        PORTBbits.RB2 = 0;
        __delay_us(1);
        
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
        sprintf(ads, "%02x", data);
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
}

// ****************************************************************************
// write to eprom
// Timing critical code. At 20MHz xtal clock, each instruction = 200nS
//
void do_write()
{
    uint16_t addr;
    char ads[32];   
    char c;
    
    // Set port D to output
    TRISD = 0x00;
        
    // Set CE2 hi to enable writing
    PORTBbits.RB1 = 1;
    // Set _RD hi
    PORTBbits.RB2 = 1;
    // Set PGM lo
    PORTBbits.RB3 = 0;
        
    for (addr = 0; addr < 2048; addr++) {
        if (cmd_active == false) {
            char *s = "Write aborted\n";
            uart_puts(s);
            return;
        }

        // The sender sends a stream of hex ascii pairs, onto a
        // stack.

        // Get two ascii chars from stack. Note they are popped
        // in order of lobyte : hibyte
        c = pop();
        uint8_t lo = charToHexDigit(c);
        c = pop();
        uint8_t hi = charToHexDigit(c);
        uint8_t data = hi*16+lo;
        
        // Put the address lines out. D0-7 is A0-7, C0-2 is A8-10
        PORTD = addr & 0x00ff;
        PORTCbits.RC0 = (addr >> 8)  & 0x01;
        PORTCbits.RC1 = (addr >> 9)  & 0x01;
        PORTCbits.RC2 = (addr >> 10) & 0x01;
        
        // Set ALE hi; AD0-7,IO/_M. A8-10, CE2 and _CE1 enter latches
        __delay_us(1);
        PORTBbits.RB0 = 1;
        __delay_us(1);
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        PORTBbits.RB0 = 0;
        __delay_us(1);
        
        // Write the byte to port D
        PORTD = data;
        
        __delay_us(10);
        
        // Set CE1 hi 
        PORTBbits.RB4 = 1;
        
        // Activate PGM pulse for 50mS
        __delay_us(1);
        PORTBbits.RB3 = 1;
        
        __delay_ms(50);
        
        // Deactivate PGM pulse
        PORTBbits.RB3 = 0;
        __delay_us(1);
        
        // Set CE1 lo
        PORTBbits.RB4 = 0;
        __delay_us(1);
     
        // We should now verify the byte is written,
        // and if necessary try writing again until ok.
        
    }
    
    // Set CE2 lo to disable writing.
    PORTBbits.RB1 = 0;
    
    sprintf(ads, "pushed: %d, popped: %d\n", bytes_pushed, bytes_popped);
    uart_puts(ads);
}

// ****************************************************************************
// main
void main(void) {
    
    // Set the receiver stack pointer
    sptr = TOP;

    // Initialise uart
    uart_init(115200);
    
    // Initialise the IO ports
    ports_init();
    
    // Loop while waiting for commands
    // We flash a green LED so we know we are listening...
    while (true) {
        PORTEbits.RE0 = 1;
        __delay_ms(250);      
        PORTEbits.RE0 = 0;
        PORTEbits.RE1 = 0;
        PORTEbits.RE2 = 0;
        __delay_ms(250);
        
        if (cmd_active) {
            // turn on orange LED to show we're active
            PORTEbits.RE1 = 1;
            
            // pop the cmd off the stack
            char cmd = top();
            
            // Do the cmd
            if      (cmd == CMD_DONE) {
                do_finish();
            }
            else if (cmd == CMD_READ) {
                do_read();
            }
            else if (cmd == CMD_WRTE) {
                do_write();
            }
            else if (cmd == CMD_CHEK) {
                do_blank();
            }

            // Clear the cmd
            clear();
        } 
    } 
}



