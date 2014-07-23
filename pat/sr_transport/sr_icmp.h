/*-----------------------------------------------------------------------------
 * file:
 * date:
 * Author:
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_ICMP_H
#define SR_ICMP_H

#ifdef _LINUX_
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#include "sr_router.h"
#include "sr_protocol.h"

uint16_t sr_calculate_icmp_checksum(struct icmp* packet/*borrowed*/);
void sr_icmp_echo_input(struct sr_core* sr,uint8_t *packet/*given*/);
void sr_icmp_input(struct sr_core* sr, uint8_t* packet);
void sr_send_icmp_echoreply(struct sr_core* sr,
                           uint8_t *packet /*given*/);
uint16_t sr_calculate_icmp_checksum(struct icmp* packet/*borrowed*/);
void sr_send_icmp_timeout(struct sr_core* sr,uint8_t *packet/*given*/);
void sr_send_icmp_no_route(struct sr_core* sr, uint8_t *packet /*given*/, 
        uint8_t why);

#endif  /* SR_ICMP_H */
