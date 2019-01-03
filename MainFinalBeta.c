#include "ADXL345.h"
#include <stdio.h>
#include <stdbool.h>
#include "VGA_Sync.h"
#include "gsensor.h"
#include "leds.h"
#include "time.h"
#include "stdlib.h"

bool check_hit(box * A, box * B){
        if( (A->y < 235) && (A->y > 205) || (A->y+10 < 235) && (A->y+10 > 210) ){
                if ( (A->x > B->x) && (A->x < (B->x + B->width)) ) {
                    printf("Left HIT\n");
                    return true;
                }
                if ( (A->x + A->width < B->x) && (A->x + A->width >  B->x + B->width) ) {
                    printf("Right HIT\n");
                    return true;
                }
        }
        return false;
}


int main(int argc, char ** argv) {
        bool game_on = true;
        int count = 0;
        int lives = 3;
        int pos;
        srand(time(NULL));

        //Initilize LEDS
        leds_init();
        //Initialize GSENSOR
        init_gsensor();
        //Initialize VGA
        VGA_init();

        //Turn 3 LEDs on
        *h2p_lw_led_addr = 7;

        box * player = new_box(150, 210, 50, 25, 2 ^ 15);

        box * E1 = new_box(10, 0, 10, 10, 0xF000);
        box * E2 = new_box(60, 0, 10, 10, 0xF000);
        box * E3 = new_box(120, 0, 10, 10, 0xF000);
        box * E4 = new_box(220, 0, 10, 10, 0xF000);

        box * life = new_box(220, 0, 10, 10, 0x0F00);

        while (game_on) {


                //Counter between location changes, acts as clock divider
                //All Moving happens in IF
                count++;
                if (count >= 6) {
                        count = 0;

                        //Makes tilt equal to gsensor X output
                        int tilt = get_gsensor_x();

                        //Move Box On Tilt
                        if ( (tilt > 0) && ((player->x + player->width) < 320) ) {
                                move_box_horizontally(player, 3);
                                //printf("Box Moved Right\n");
                        }
                        else if ( (tilt < 0) && (player->x > 1) ) {
                                move_box_horizontally(player, -3);
                                //printf("Box Moved Left\n");
                        }
                        else {
                                //printf("Box Not Moved\n");
                                move_box_horizontally(player, 0);
                        }

                        //Enemy 1 Move
                        if (check_hit(E1,player)) {
                                //printf("%i", E1hit);
                                lives--;
                                *h2p_lw_led_addr = *h2p_lw_led_addr >> 1;
                                move_box_vertically(E1, -(E1->y));
                                pos=(  rand()%301 - (E1->x)  );
                                move_box_horizontally(E1,pos);
                        }
                        else if ( (E1->y) < 250)  {
                                move_box_vertically(E1, 2);
                                //E1hit = check_hit(E1,player);
                        }
                        else {
                                move_box_vertically(E1, -250);
                                pos=(  rand()%300 - (E1->x)  );
                                move_box_horizontally(E1,pos);

                        }

                        //Enemy 2 Move
                        if (check_hit(E2,player)) {
                                //printf("%i", E1hit);
                                lives--;
                                *h2p_lw_led_addr = *h2p_lw_led_addr >> 1;
                                move_box_vertically(E2, -(E2->y));
                                pos=(  rand()%301 - (E2->x)  );

                                move_box_horizontally(E2,pos);
                        }
                        else if ( (E2->y) < 250)  {
                                move_box_vertically(E2, 6);
                        }
                        else {
                                move_box_vertically(E2, -250);
                                pos=(  rand()%301 - (E2->x)  );
                                move_box_horizontally(E2,pos);
                        }

                        //Enemy 3 Move
                        if (check_hit(E3,player)) {
                                //printf("%i", E1hit);
                                lives--;
                                *h2p_lw_led_addr = *h2p_lw_led_addr >> 1;
                                move_box_vertically(E3, -(E3->y));
                                pos=(  rand()%301 - (E3->x)  );
                                move_box_horizontally(E3,pos);
                        }
                        else if ( (E3->y) < 250)  {
                                move_box_vertically(E3, 4);
                        }
                        else {
                                move_box_vertically(E3, -250);
                                pos=(  rand()%301 - (E3->x)  );
                                move_box_horizontally(E3,pos);
                        }

                        //Enemy 4 Move
                        if (check_hit(E4,player)) {
                                //printf("%i", E1hit);
                                lives--;
                                *h2p_lw_led_addr = *h2p_lw_led_addr >> 1;
                                move_box_vertically(E4, -(E4->y));
                                pos=(  rand()%301 - (E4->x)  );

                                move_box_horizontally(E4,pos);
                        }
                        else if ( (E4->y) < 250)  {
                                move_box_vertically(E4, 5);
                        }
                        else {
                                move_box_vertically(E4, -250);
                                pos=(  rand()%300 - (E4->x)  );

                                move_box_horizontally(E4,pos);
                        }

                        if (lives < 1) {
                            printf("\n\nYou are a \n");
                            printf("      L    O    S    E    R!\n\n");
                            game_on = false;
                        }

                        //Go to next frame -- switches between buffers
                        push_frame(periph_virtual_base);
                        //Switch Back Buffer to ensure next move is written to $
                        bb = !bb;

                }

                //Print Player Location
                //printf("Player Location: X: %i  Y: %i\n", player->x, player->$

                //Exit Clause NEEDED (currently can only suspend to exit)

                //Add One LED (one death)
                //Put in Death IF
        }

        close_gsensor();
        close_leds;
        VGA_UnMap_VirtualBoxes();
}
