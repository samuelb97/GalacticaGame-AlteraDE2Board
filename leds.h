#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

#define HW_REGS_BASE_LED ( 0xff200000 )
#define HW_REGS_SPAN_LED ( 0x00200000 )
#define HW_REGS_MASK_LED ( HW_REGS_SPAN_LED - 1 )
#define LED_PIO_BASE 0x0
#define SW_PIO_BASE 0x40

#define ALLON  1023
#define ALLOFF 0

volatile sig_atomic_t stop_leds;

volatile unsigned int *h2p_lw_led_addr=NULL; // address to the leds
volatile unsigned int *h2p_lw_sw_addr=NULL; // address to the switches
void *virtual_base;
int fd;

void catchSIGINT_2(int signum){
    stop_leds = 1;
}

void leds_init() {
    // catch SIGINT from ctrl+c, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT_2);
    
    // Open /dev/mem
    if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
        printf( "ERROR: could not open \"/dev/mem\"...\n" );
        exit( 1 );
    }
    
    // get virtual addr that maps to physical
    virtual_base = mmap( NULL, HW_REGS_SPAN_LED, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE_LED );    
    if( virtual_base == MAP_FAILED ) {
        printf( "ERROR: mmap() failed...\n" );
        close( fd );
        exit(1);
    }
    
    // Get the address that maps to the LEDs
    h2p_lw_led_addr=(unsigned int *)(virtual_base + (( LED_PIO_BASE ) & ( HW_REGS_MASK_LED ) ));
    h2p_lw_sw_addr=(unsigned int *)(virtual_base + (( SW_PIO_BASE ) & ( HW_REGS_MASK_LED ) ));

    

    printf("Running leds. To exit, press Ctrl+C.\n");

}

void close_leds() {
    printf("\nclosing\n");
    *h2p_lw_led_addr = 0;
    if( munmap( virtual_base, HW_REGS_SPAN_LED ) != 0 ) {
        printf( "ERROR: munmap() failed...\n" );
        close( fd );
        exit( 1 );

    }
    close( fd );
}