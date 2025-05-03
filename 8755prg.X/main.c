// ****************************************************************************
//
// Project              : 8755prg. 8755 / 8748 programmer
// File                 : main.c
// Hardware Environment : PIC 16F1789
//                        5v supply voltage
//                        internal oscillator
// Build Environment    : MPLAB IDE
// Version              : V8.76
// By                   : Keith Sabine (keith@peardrop.co.uk)
// Created on           : April 10, 2024, 12:47 PM
//
// ****************************************************************************

#include "conbits.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "uart.h"

// Useful defines
#define INPUT  0xFF
#define OUTPUT 0x00

#define DEV_2716 0
#define DEV_2732 1
#define DEV_2532 2
#define DEV_2708 3
#define DEV_T2716 4
#define DEV_8755 5
#define DEV_8748 6
// cmds
#define CMD_READ '1'               // Read from the EPROM
#define CMD_WRTE '2'               // Program the EPROM
#define CMD_CHEK '3'               // Check EPROM is blank (all FF))
#define CMD_IDEN '4'               // Get the ID of the device ("8755")
#define CMD_TYPE '5'               // Set the device type
#define CMD_RSET '9'               // Reset the PIC
#define CMD_INIT 'U'               // init the baud rate

// Received chars are put into a queue.
// See e.g. Aho, Hopcroft & Ullman, 'Data structures and Algorithms'
#define QUEUESIZE 1024             // Queue size
#define ENDQUEUE  QUEUESIZE-1      // End of queue
#define HIWATER   QUEUESIZE-32     // The highwater mark, stop sending.
#define LOWATER   32               // The lowwater mark, resume sending.

//
// static variables
//
static char    queue[QUEUESIZE];   // The receiver queue
static int16_t head = 0;           // head of the queue
static int16_t tail = ENDQUEUE;    // tail of the  queue
static bool    cmd_active = false; // Are we in a cmd?
static bool    queue_empty = false;// wait if queue empty
static int8_t  devType = 5;        // 5 = 8755, 6 = 8748
static int16_t bytes = 1024;       // size of program data
static bool    writing = false;    // are we programming?

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
void clear()
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
    if (s < LOWATER) {
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
    // If the queue is nearly full, set CTS.)
    int16_t s = addone(tail) - head;
    if (s > HIWATER) {
        setCTS(true);
    }
    else {
        setCTS(false);
    }
        
    if ( addone(addone(tail)) == head) {
        // error - queue is full. Flash red led.
        LATEbits.LATE2 = 1;
        __delay_ms(100);
        LATEbits.LATE2 = 0;
        __delay_ms(100);
    }
    else {
        tail = addone(tail);
        queue[tail] = c;
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
    // Check for empty. Do this before disabling interrupts
    // as need to receive chars still.
    while (empty()) {
        // Wait for queue to fill, flash green led.
        LATEbits.LATE0 = 1;
        __delay_ms(100);
        LATEbits.LATE0 = 0;
        __delay_ms(100);
    }

    // Disable interrupts
    INTCONbits.GIE = 0;
    PIE1bits.RCIE=0;

    // Get the head of the queue.
    char c = queue[head];
    head = addone(head);
    
    // Enable interrupts
    INTCONbits.GIE = 1;
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
    ANSELA = 0;           // Port A all digital
    ANSELB = 0;           // Port B all digital
    ANSELC = 0;           // Port C all digital
    ANSELD = 0;           // Port D all digital
    ANSELE = 0;           // Port E all digital
    
    // Use port E for status LEDs
    TRISEbits.TRISE0 = 0; // green  LED, while loop
    TRISEbits.TRISE1 = 0; // orange LED, interrupt
    TRISEbits.TRISE2 = 0; // red    LED, warning
    PORTEbits.RE0 = 0;
    PORTEbits.RE1 = 0;
    PORTEbits.RE2 = 0;
    
    // Port RA0 = SELECT (hi for 8748)
    // Port RA1 = EA (8748 only, else kept lo)
    // Port RA2 = CTS
    // Port RA3 = RTS
    // Port RA4 = PROG
    // Port RA5 unused
    // Port RA6/7 XTAL
    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 0;
    TRISAbits.TRISA2 = 0; // CTS is an active low output
    TRISAbits.TRISA3 = 1; // RTS is an active low input
    TRISAbits.TRISA4 = 0;
    TRISAbits.TRISA5 = 0;
    LATAbits.LATA0    = 0; // select 8755
    LATAbits.LATA1    = 0; // EA lo
    LATAbits.LATA2    = 0; // assert CTS
    LATAbits.LATA4    = 0; // PROG lo

    // Port D output for address/data bits AD0-AD7
    TRISD = OUTPUT;
    PORTD = 0;
    
    // Port C bits 0,1,2,3 = A8-A11 as outputs
    // Port RC4,5 unused
    // Port RC6,7 uart tx/rx
    TRISC = 0b11000000;
    
    TRISB = OUTPUT;
    // Port B0 = ALE
    // Port B1 = CE2
    // Port B2 = RD_ (made input for 8748 as PSEN_ is an output))
    // Port B3 = VDD (switches +25v onto VDD pin)
    // Port B4 = CE1_
    // Port RB5 = RESET/RESET_
    // Port RB6 unused
    // Port RB7 unused
    LATBbits.LATB0 = 0; // set ALE false
    LATBbits.LATB1 = 0; // set CE2 false
    LATBbits.LATB2 = 1; // set RD_ false
    LATBbits.LATB3 = 0; // set VDD +5v
    LATBbits.LATB4 = 1; // set CE1_ false
    LATBbits.LATB5 = 0; // set RESET false for 8755
}

// ****************************************************************************
// Set the device type and the RE0/1 bits
//
void
do_type()
{
    devType = (int8_t) pop() - (int8_t) '0';
            
    if (devType == DEV_8755) {
        bytes = 2048;          // 8755 has 2K EPROM
        LATAbits.LATA0 = 0;    // SEL
        LATBbits.LATB5 = 0;    // RESET
        TRISBbits.TRISB2 = 0;  // RD_ is an O/P from PIC
        LATBbits.LATB2 = 1;    // RD_ set false
    } else 
    if (devType == DEV_8748) {
        bytes = 1024;          // 8748 has 1K EPROM
        LATAbits.LATA0 = 1;    // SEL
        LATBbits.LATB5 = 0;    // RESET_
        TRISBbits.TRISB2 = 1;  // PSEN is an O/P
        LATBbits.LATB2 = 0;    // RD_/PSEN
    }
    
    uart_puts("OK");
}

// ****************************************************************************
// high priority service routine for UART receive
//
void __interrupt() isr(void)
{
    char c = 0;

    // Disable interrupts
    INTCONbits.GIE = 0;
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
    INTCONbits.GIE = 1;
}

// ****************************************************************************
// Set the address on ports D and C, and load by pulsing ALE.
//
void setup_address(uint16_t addr)
{
    // We get here with
    // T0/CE1_ lo
    // CE2     hi
    // PGM     lo
    // RESET_  lo (if 8748)
    // EA      hi (if 8748)
    // T0      hi (if 8748)
    
    // Set port D to output address
    TRISD = OUTPUT;

    if (devType == DEV_8755) {
        // Set _RD hi
        LATBbits.LATB2 = 1;
    }

    // Set the address lines. D0-7 is A0-7, C0-2 is A8-10
    uint8_t hi = addr >> 8;
    LATD       = addr & 0x00ff;
    LATC       = hi;
    __delay_us(5);

    // If we're an 8755
    if (devType == DEV_8755) {
        // Set ALE hi; AD0-7,IO/_M. A8-10, CE2 and _CE1 enter latches
        LATBbits.LATB0 = 1;
        __delay_us(2);
        // Set ALE lo, latches AD0-7,A8-10, CE2 and _CE1
        LATBbits.LATB0 = 0;
        __delay_us(2);
    }
    else {
        // 8748 set RESET_ hi to latch address
        // Use a delay of 4*tcy where tcy = 5us for 8748-8
        LATBbits.LATB5 = 0;
        __delay_us(20);
        LATBbits.LATB5 = 1;
        __delay_us(20);
    }
}

// ****************************************************************************
// Read a byte from port D
//
uint8_t read_port()
{
    // Set port D to input to read from DUT
    TRISD = INPUT;
    __delay_us(5);
    
    // Set _RD_ lo to enable reading in 8755
    if (devType == DEV_8755) {
        LATBbits.LATB2 = 0;
        __delay_us(1);
    }
    else {
        // 8748, could be 5us
        __delay_us(50);
    }

    // Read port D
    uint8_t data = PORTD;

    // Set _RD hi to disable reading in 8755
    if (devType == DEV_8755) {
        __delay_us(1);
        LATBbits.LATB2 = 1;
    }
    else {
        // Set RESET_ lo
        LATBbits.LATB5 = 0;
        __delay_us(5);
    }

    // Set port D back to output 
    TRISD = OUTPUT;
    
    return data;
}

// ****************************************************************************
// Init uart baud rate by waiting for a 'U' char
//
void do_init()
{
    uint16_t rate;
    char s[8];
        
    rate = uart_init_brg();
    
    sprintf(s, "%d\n", rate);
    uart_puts(s);
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
        
    // Set CE1_ lo - enabled
    LATBbits.LATB4 = 0;    
    // Set CE2 hi - enabled
    LATBbits.LATB1 = 1;
    // Set PGM lo - disabled
    LATBbits.LATB3 = 0;
        
    for (addr = 0; addr < bytes; ++addr) {
        if (cmd_active == false) {
            uart_puts("Check aborted\n");
            return;
        }

        if (devType == DEV_8748) {
            // Set RESET_ lo
            LATBbits.LATB5 = 0;
            // Set EA to read from program memory
            LATAbits.LATA1 = 1;
            // T0 hi (verify mode))
            LATBbits.LATB4 = 1;
        }
        
        // Latch the 16 bit address.
        setup_address(addr);
        
        // Read port D
        uint8_t data = read_port();
   
        // clear EA
        LATAbits.LATA1 = 0;
        
        if (data != 0xff) {
            uart_puts("Erase check fail at address ");
            sprintf(ads, "0x%04x = ", addr);
            uart_puts(ads);
            sprintf(ads, "0x%02x\n", data);
            uart_puts(ads);
            ok = false;
            break;
        }
    }
    
    // Set CE2 lo - disable
    LATBbits.LATB1 = 0;
    
    if (ok) {
        uart_puts("OK");
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
    
    // Set CE1_ lo - enabled
    LATBbits.LATB4 = 0;    
    // Set CE2 hi - enabled
    LATBbits.LATB1 = 1;
    // Set PGM lo - disabled
    LATBbits.LATB3 = 0;
        
    for (addr = 0; addr < bytes; ++addr) {
        if (cmd_active == false) {
            uart_puts("Read aborted\n");
            return;
        }
        
        if (devType == DEV_8748) {
            // Set RESET_ lo
            LATBbits.LATB5 = 0;
            // Set EA to read from program memory
            LATAbits.LATA1 = 1;
            // T0 hi (verify mode))
            LATBbits.LATB4 = 1;
        }
        
        // Latch the 16 bit address.
        setup_address(addr);
    
        // Read port D
        uint8_t data = read_port();
        
        // clear EA
        LATAbits.LATA1 = 0;
        
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
    
    // Set CE2 lo - disable
    LATBbits.LATB1 = 0;
}

// ****************************************************************************
// Write a byte
//
void write_port(uint8_t data)
{
    // Write the byte to port D
     __delay_us(10);
    LATD = data;
 
    if (devType == DEV_8755) {

        // Set CE1 hi 
        __delay_us(10);
        LATBbits.LATB4 = 1;
    
        // Activate PGM pulse for 50mS
        __delay_us(2);
        LATBbits.LATB3 = 1;

        __delay_ms(50);

        // Deactivate PGM pulse
        LATBbits.LATB3 = 0;
        __delay_us(2);
    
        // Set CE1 lo
        LATBbits.LATB4 = 0;
        __delay_us(1);
    
    } 
    else if (devType == DEV_8748) {
    
        // Set TO lo
        __delay_us(2);
        LATBbits.LATB4 = 0;
        
        // Activate VDD pulse
        __delay_us(20);
        LATBbits.LATB3 = 1;

        // Activate PROG pulse for 50ms
        LATAbits.LATA4 = 1;

        __delay_ms(50);

        // Deactivate PROG pulse
        LATAbits.LATA4 = 0;

        // Deactivate VDD pulse
        LATBbits.LATB3 = 0;
        __delay_us(20);
        
        // Set TO hi
        __delay_us(2);
        LATBbits.LATB4 = 1;
    
    }
}

// ****************************************************************************
// write to eprom
// Timing critical code. At 20MHz xtal clock, each instruction = 200nS
//
void do_write()
{
    uint16_t addr;
    char c;
    
    // Set write mode
    writing = true;
    
    // Set port D to output
    TRISD = OUTPUT;
        
    // Wait for a couple of chars before starting write
    __delay_ms(100);
    
    // Set CE2 hi - enable
    LATBbits.LATB1 = 1;
    // Set _RD hi - disable
    LATBbits.LATB2 = 1;
    // Set PGM lo - disable
    LATBbits.LATB3 = 0;
    
    if (devType == DEV_8748) {
        // Set EA to hi
        LATAbits.LATA1 = 1;
    }
        
    for (addr = 0; addr < bytes; addr++) {
        if (cmd_active == false) {
            uart_puts("Write aborted\n");
            return;
        }

        // Get two ascii chars from queue and convert to 8 bit data.
        c = pop();
        uint8_t hi = charToHexDigit(c);
        c = pop();
        uint8_t lo = charToHexDigit(c);
        uint8_t data = hi*16+lo;
        
        // Latch the 16 bit address.
        setup_address(addr);
        
        // Write the byte to port D
        write_port(data);
    }
    
    if (devType == DEV_8748) {
        // Set EA to lo
        LATAbits.LATA1 = 0;
    }
    
    // Set CE2 lo - disable
    LATBbits.LATB1 = 0;
    
    // unset write mode
    writing = false;
    
    uart_puts("OK");
}

// ****************************************************************************
// main
void main(void) {

    // Initialise uart. A value of 0 means use auto baud rate detection.
    uart_init(0);
    
    // Initialise the IO ports
    ports_init();
    
    // Wait for a 'U' char to init the uart BRG
    do_init();
    
    // Enable interrupts
    PIE1bits.RCIE=1;
    INTCONbits.GIE = 1;
        
    // Loop while waiting for commands
    // We flash a green LED so we know we are listening...
    while (true) { 
        if (cmd_active) {
            // Turn on orange LED to show we're active
            LATEbits.LATE0 = 0; // green off
            LATEbits.LATE1 = 1; // orange on
            
            // pop the $
            pop();
            // and the cmd
            char cmd = pop();
            
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
            else if (cmd == CMD_INIT) {
                uart_puts("Already init");
            }
            else if (cmd == CMD_TYPE) {
                do_type();
            }
            else if (cmd == CMD_IDEN) {
                if (devType == 5)
                    uart_puts("8755");
                if (devType == 6)
                    uart_puts("8748");
                else
                    uart_puts("NONE");
            }
            else if (cmd == CMD_RSET) {
                asm("RESET");
            }

            // Clear the cmd
            clear();
        } 
        else {
            // Green light to show we're ready
            LATEbits.LATE0 = 1; // green on
            LATEbits.LATE1 = 0; // orange off
        }
        
        // Delay for the loop
        __delay_us(10);  
    } 
}



