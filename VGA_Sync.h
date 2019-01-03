#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

//Global Variables
int fd;
void *sdram_virtual_base;
void *periph_virtual_base;
//unsigned int x = BOX_INIT_X, y = BOX_INIT_Y;
//unsigned int x_cnt = 0, y_cnt = 0;
int x_dir = 1;
int y_dir = 1;
bool bb = false;

#define HW_REGS_BASE_VGA ( 0xFC000000 )
#define HW_SDRAM_BASE ( 0xC0000000 )
#define HW_REGS_SPAN_VGA ( 0x04000000 )
#define HW_REGS_MASK_VGA ( HW_REGS_SPAN_VGA - 1 )
#define PHYSMEM_32(virtual_base,addr) (*((unsigned int *)(virtual_base + (addr & HW_REGS_MASK_VGA))))
#define PHYSMEM_16(virtual_base,addr) (*((unsigned short *)(virtual_base + (addr & HW_REGS_MASK_VGA))))

// VIDEO BUFFER MEMORY CONFIG
#define BUFFER_ADDR 0xC0000000
#define BACK_BUFFER_ADDR 0xC0100000
#define BUFFER_ADDR_REG 0xFF203020
#define BACK_BUFFER_ADDR_REG 0xFF203024
#define DMA_CTRL_REG 0xFF20302C
#define RES_X 320
#define RES_Y 240
#define BITS_X 9
#define BITX_Y 8
#define USE_DOUBLE_BUFFERING 1

// ANIMATION PARAMS
#define BG_COLOR 0x0
#define BOX_COLOR 0xf00f
#define BOX_WIDTH 10
#define BOX_HEIGHT 10
#define BOX_INIT_X 0
#define BOX_INIT_Y 0
#define X_TRANS_CYCLES 100000
#define Y_TRANS_CYCLES 100000

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

void clear_screen(void* sdram_virt_base, void* periph_virt_base){
    unsigned int row, col, pixel_ptr;
    #if USE_DOUBLE_BUFFERING == 1
    unsigned int pixel_ptr_base = PHYSMEM_32(periph_virt_base,BACK_BUFFER_ADDR_REG);
    #else
    unsigned int pixel_ptr_base = BUFFER_ADDR;
    #endif
    for (row = 0; row < RES_Y; row++){
        for (col = 0; col < RES_X; ++col)
        {
            pixel_ptr = pixel_ptr_base | (row << (BITS_X+1)) | (col << 1);
            PHYSMEM_16(sdram_virt_base,pixel_ptr) = BG_COLOR;
        }
    }
}

/* Draw a box whose top left corner is (x1, y1). box defined by BOX_WIDTH, BOX_HEIGHT */
void draw_box(void* sdram_virt_base, void* periph_virt_base, unsigned short color, unsigned int x1, unsigned int y1, unsigned int width, unsigned int height){
    unsigned int row, col, pixel_ptr;
    #if USE_DOUBLE_BUFFERING == 1
    unsigned int pixel_ptr_base = PHYSMEM_32(periph_virt_base,BACK_BUFFER_ADDR_REG);
    #else
    unsigned int pixel_ptr_base = BUFFER_ADDR;
    #endif
    for (row = y1; row < y1 + height; row++){
        for (col = x1; col < x1 + width; ++col)
        {
            pixel_ptr = pixel_ptr_base | (row << (BITS_X+1)) | (col << 1);
            PHYSMEM_16(sdram_virt_base,pixel_ptr) = color;
        }
    }
}

void push_frame(void* periph_virt_base){
    // swap the buffers to draw the new frame
    PHYSMEM_32(periph_virt_base,BUFFER_ADDR_REG) = 1;
    while(PHYSMEM_32(periph_virt_base,DMA_CTRL_REG) & 1){}
}


typedef struct box{
	unsigned int x, y, width, height;
	unsigned short color;
	unsigned int x_prev_bb, y_prev_bb;
	unsigned int x_prev_fb, y_prev_fb;
} box;

//Box Constructor
box * new_box(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned short color) {
	box * temp = malloc(sizeof(box));
	temp->x = x;
	temp->y = y;
	temp->width = width;
	temp->height = height;
	temp->color = color;
	temp->x_prev_bb = x;
	temp->y_prev_bb = y;
	temp->x_prev_fb = x;
	temp->y_prev_fb = y;

	//Draws Box on Peripheral
	draw_box(sdram_virtual_base, periph_virtual_base, color, x, y, width, height);

	return temp;
}

void move_box_vertically(box * A, unsigned int change) {
	A->y = A->y + change;
	//Erases Box by drawing over with BackGround Color
	draw_box(sdram_virtual_base, periph_virtual_base, BG_COLOR, bb ? A->x_prev_bb : A->x_prev_fb, bb ? A->y_prev_bb : A->y_prev_fb, A->width, A->height);
 	//Draws New Box
	draw_box(sdram_virtual_base, periph_virtual_base, A->color, A->x, A->y, A->width, A->height);

	if (bb) {
		A->x_prev_bb = A->x;
		A->y_prev_bb = A->y;
	}
	else {
		A->x_prev_fb = A->x;
		A->y_prev_fb = A->y;
	}
}

void move_box_horizontally(box * A, unsigned int change) {
	A->x = A->x + change;
	//Erases Box by drawing over with BackGround Color
	draw_box(sdram_virtual_base, periph_virtual_base, BG_COLOR, bb ? A->x_prev_bb : A->x_prev_fb, bb ? A->y_prev_bb : A->y_prev_fb, A->width, A->height);
 	//Draws New Box
	draw_box(sdram_virtual_base, periph_virtual_base, A->color, A->x, A->y, A->width, A->height);

	if (bb) {
		A->x_prev_bb = A->x;
		A->y_prev_bb = A->y;
	}
	else {
		A->x_prev_fb = A->x;
		A->y_prev_fb = A->y;
	}
}


int VGA_init() {
	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
	signal(SIGINT, catchSIGINT);

	// Open up the system memory file
	if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
		printf("ERROR: could not open \"/dev/mem\"...\n");
		return(1);
	}
	// Map peripherals region (lwhps2fpga bridge) to virtual address. The buffer buffer core is here.
	periph_virtual_base = mmap(NULL, HW_REGS_SPAN_VGA, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE_VGA);
	if (periph_virtual_base == MAP_FAILED) {
		printf("ERROR: mmap() failed...\n");
		close(fd);
		return(1);
	}

	// Map sdram region. We will use the SDRAM as the pixel buffer.
	sdram_virtual_base = mmap(NULL, HW_REGS_SPAN_VGA, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_SDRAM_BASE);
	if (sdram_virtual_base == MAP_FAILED) {
		printf("ERROR: mmap() failed...\n");
		close(fd);
		return(1);
	}

	// Set front/backbuffer addrs, clear them
	PHYSMEM_32(periph_virtual_base, BACK_BUFFER_ADDR_REG) = BUFFER_ADDR;
	clear_screen(sdram_virtual_base, periph_virtual_base);
	push_frame(periph_virtual_base);
	#if USE_DOUBLE_BUFFERING == 1
	PHYSMEM_32(periph_virtual_base, BACK_BUFFER_ADDR_REG) = BACK_BUFFER_ADDR;
	clear_screen(sdram_virtual_base, periph_virtual_base);
	push_frame(periph_virtual_base);
	#else
	PHYSMEM_32(periph_virtual_base, BACK_BUFFER_ADDR_REG) = BUFFER_ADDR;
	#endif
}

int VGA_UnMap_VirtualBoxes() {
	if (munmap(periph_virtual_base, HW_REGS_SPAN_VGA) != 0) {
		printf("ERROR: munmap() failed...\n");
		close(fd);
		return(1);
	}
	if (munmap(sdram_virtual_base, HW_REGS_SPAN_VGA) != 0) {
		printf("ERROR: munmap() failed...\n");
		close(fd);
		return(1);
	}

	close(fd);
}

/*
/// Test program that flashes the screen with different colours
/// Uses the onchip ram on the FPGA. ASSUMES THE FPGA HAS BEEN
/// PROGRAMMED WITH THE DE1-SoC_Computer.rbf file!!!
int main(int argc,char ** argv) {
	
    int fd;
    void *sdram_virtual_base;
    void *periph_virtual_base;
    unsigned int x = BOX_INIT_X, y = BOX_INIT_Y;
    unsigned int x_cnt = 0, y_cnt = 0;
    unsigned int prev_x_fb = BOX_INIT_X, prev_y_fb = BOX_INIT_Y, prev_x_bb = BOX_INIT_X, prev_y_bb = BOX_INIT_Y;
    int x_dir = 1;
    int y_dir = 1;
    bool bb = false;
    
    // catch SIGINT from ctrl+c, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);
    
    // Open up the system memory file
    if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
	    printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return( 1 );
	}
    // Map peripherals region (lwhps2fpga bridge) to virtual address. The buffer buffer core is here.
	periph_virtual_base = mmap( NULL, HW_REGS_SPAN_VGA, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE_VGA );
	if( periph_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap() failed...\n" );
		close( fd );
		return( 1 );
	}
    
    // Map sdram region. We will use the SDRAM as the pixel buffer.
    sdram_virtual_base = mmap( NULL, HW_REGS_SPAN_VGA, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_SDRAM_BASE );
	if( sdram_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap() failed...\n" );
		close( fd );
		return( 1 );
	}
    
    // Set front/backbuffer addrs, clear them
    PHYSMEM_32(periph_virtual_base,BACK_BUFFER_ADDR_REG) = BUFFER_ADDR;
    clear_screen(sdram_virtual_base,periph_virtual_base);
    push_frame(periph_virtual_base);
    #if USE_DOUBLE_BUFFERING == 1
    PHYSMEM_32(periph_virtual_base,BACK_BUFFER_ADDR_REG) = BACK_BUFFER_ADDR;
    clear_screen(sdram_virtual_base,periph_virtual_base);
    push_frame(periph_virtual_base);
    #else
    PHYSMEM_32(periph_virtual_base,BACK_BUFFER_ADDR_REG) = BUFFER_ADDR;
    #endif
    
    // Main animation loop
    while(!stop){
        
        x_cnt++;
        y_cnt++;
        
        if (x_cnt >= X_TRANS_CYCLES){
            x_cnt = 0;
            x+=x_dir;
        }
        if (y_cnt >= Y_TRANS_CYCLES){
            y_cnt = 0;
            y+=y_dir;
        }
        
        // If box location has changed
        if (x_cnt == 0 || y_cnt == 0){
        
            // On hitting boundaries, change direction(s)
            if (y+BOX_HEIGHT >= RES_Y) y_dir = -1;
            if (y == 0) y_dir = 1;
            if (x+BOX_WIDTH >= RES_X) x_dir = -1;
            if (x == 0) x_dir = 1;
            // Draw the box at the updated location (first erase old box)
            draw_box(sdram_virtual_base,periph_virtual_base,BG_COLOR,bb?prev_x_bb:prev_x_fb,bb?prev_y_bb:prev_y_fb);
            // clear_screen(sdram_virtual_base,periph_virtual_base);
            draw_box(sdram_virtual_base,periph_virtual_base,BOX_COLOR,x,y);
            
            // #if USE_DOUBLE_BUFFERING == 1
            push_frame(periph_virtual_base);
            // #endif
            
            if (bb){
                prev_x_bb = x;
                prev_y_bb = y;
            } else {
                prev_x_fb = x;
                prev_y_fb = y;
            }
            bb = !bb;
        }
    }
                
    if( munmap( periph_virtual_base, HW_REGS_SPAN_VGA ) != 0 ) {
		printf( "ERROR: munmap() failed...\n" );
		close( fd );
		return( 1 );
	}
	if( munmap( sdram_virtual_base, HW_REGS_SPAN_VGA ) != 0 ) {
		printf( "ERROR: munmap() failed...\n" );
		close( fd );
		return( 1 );
	}

	close( fd );

	return( 0 );

}
*/