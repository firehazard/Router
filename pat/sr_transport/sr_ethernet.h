/*-----------------------------------------------------------------------------
 * file:
 * date:
 * Author:
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

struct sr_core;

void sr_ethernet_output(struct sr_core* sr,
                        const char* dest_mac,
                        uint8_t* packet        /* borrowed */,
                        char* interface);
void sr_ethernet_input(struct sr_core* sr, 
                       const uint8_t * packet/* borrowed */,
                       unsigned int len,
                       const char* interface/* borrowed */);
