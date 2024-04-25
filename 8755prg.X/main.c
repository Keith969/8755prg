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
#define CMD_READ '1'               // Read from the EPROM
#define CMD_WRTE '2'               // Program the EPROM
#define CMD_CHEK '3'               // Check EPROM is blank (all FF))

// Received chars are put into a queue.
// See e.g. Aho, Hopcroft & Ullman, 'Data structures and Algorithms'
#define QUEUESIZE 1024             // Queue size
#define ENDQUEUE  QUEUESIZE-1      // End of queue
#define HIWATER   QUEUESIZE-16     // The highwater mark, stop sending.

//
// static variables
//
static char    queue[QUEUESIZE];   // The receiver queue
static int16_t head = 0;           // head of the queue
static int16_t tail = ENDQUEUE;    // tail of the  queue

static bool    cmd_active = false; // Are we in a cmd?
static int16_t bytes_pushed = 0;   // pushed into queue
static int16_t bytes_popped = 0;   // popped from queue

// ****************************************************************************
// setCTS()
// Note CTS is active low. So setCTS(1) means 'stop sending'
//
void setCTS(bool b)
{
    PORTAbits.RA2 = b;
}

// ****************************************************************************
// reset the queue
//
void makenull()
{
    memset(queue, 0x00, QUEUESIZE);
    head = 0;
    tail = ENDQUEUE; 
    cmd_active   = false;
}

// ****************************************************************************
// Get the next position clockwise in array, handling begin/end of array.
//
int16_t addone(int16_t i)
{
    if (i == ENDQUEUE)
        return 0;
    return i+1;
}

// ****************************************************************************
// How many items are in the queue? Set CTS if more than hiwater mark.
//
int16_t size()
{
    int16_t s = addone(tail) - head;
    if (s > HIWATER) {
        setCTS(true);
    }
    else {
        setCTS(false);
    }
    return s;
}

// ****************************************************************************
// Is the queue empty?
// An empty queue has head one more (clockwise) than tail.
//
bool empty()
{
    if (addone(tail) == head)
        return true;
    return false;
}
// ****************************************************************************
// push a char onto queue.
// to enqueue (push), we move tail one position clockwise.
// e.g. head=0, tail = ENDQUEUE. after push, 
//   queue[0]=c
//   head = 0;
//   tail = 0;
//
void push(char c)
{    
    if ( addone(addone(tail)) == head) {
        // error - queue is full. Set error led.
        // TODO: return false if queue full.
        while (true)
            TRISEbits.RE2 = 1;
    }
    else {
        tail = addone(tail);
        queue[tail] = c;
        bytes_pushed++;
    }  
}

// ****************************************************************************
// pop a char from queue. 
// to dequeue, we move head one position clockwise.
// e.g. after one push, head=0,tail=0
// c = queue[0]
// head = 1;
// tail = 0; (note that now queue is empty as tail+1 == head))
//
char pop()
{
    // pop() is called in write() and could be interrupted, which would
    // cause havoc to the queue. So disable interrupts.)
    INTCONbits.GIEH = 0;
    PIE1bits.RCIE=0;
    
    char c = 0;
    
    if (empty()) {
        // error - queue is empty.  Set error led.
        // TODO: return false if empty.
        while (true)
            TRISEbits.RE2 = 1;
    }
    else {
        c = queue[head];
        head = addone(head);
        bytes_popped++;
    }
    
    // Enable interrupts
    INTCONbits.GIEH = 1;
    PIE1bits.RCIE = 1;
    return c;
}

// ****************************************************************************
// first - get the first char pushed on the queue, without removing it.
char first()
{
    return queue[head];
}

// ****************************************************************************
// convert char to hex digit. Handle upper and lower case.
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

        // Check if we have a cmd yet. 
        int16_t n = size();
        if ( (first() == '$') && n > 1) {
            // We have a command (2 chars at head of queue))
            cmd_active = true;
        }
    }
    
    // Enable interrupts
    PIE1bits.RCIE=1;
    INTCONbits.GIEH = 1;
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
        __delay_us(1);
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        PORTBbits.RB0 = 0;
        __delay_us(1);
        
        // Set port D to input
        TRISD = 0xff;
        // Set _RD_ lo to enable reading
        PORTBbits.RB2 = 0;
        __delay_us(1);
        
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
        
        // Read port D
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

        // Get two ascii chars from queue. Note they are popped
        // in order of lobyte : hibyte TODO: are they?
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
            
            // pop the $
            char cmd = pop();
            // and the cmd
            cmd = pop();
            
            // Do the cmd
            if      (cmd == CMD_READ) {
                do_read();
            }
            else if (cmd == CMD_WRTE) {
                do_write();
            }
            else if (cmd == CMD_CHEK) {
                do_blank();
            }

            // Clear the cmd
            makenull();
        } 
    } 
}



