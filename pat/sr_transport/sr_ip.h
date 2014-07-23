/*-----------------------------------------------------------------------------
 * file:
 * date:
 * Author:
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_IP_H
#define SR_IP_H

#ifdef _LINUX_
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#include "sr_if.h"
#include "sr_protocol.h"

struct sr_core; /* -- forwards declaration -- */

/*-----------------------------------------------------------------------------
 * Component: ip_input(..) 
 *---------------------------------------------------------------------------*/

void sr_ip_input (struct sr_core* sr,     /* -- borrowed -- */
                  uint8_t* packet,            /* -- given -- */
                  int len,
                  const struct sr_if* iface); /* -- borrowed -- */

uint16_t sr_calculate_ip_checksum(struct ip* ip_hdr/*borrowed*/);
void     sr_print_ip_hdr(struct ip* hdr/*borrowed*/);

/*-----------------------------------------------------------------------------
 * Component: ip_output(..) 
 *---------------------------------------------------------------------------*/

void sr_ip_output(struct sr_core* sr,
                       uint8_t* packet /* given */,
                       uint8_t protocol,
                       uint32_t dest, /*nbo*/
                       int len);

/*-----------------------------------------------------------------------------
 * Component: ip_forward(..) 
 *---------------------------------------------------------------------------*/

uint8_t  sr_decrement_ip_TTL(uint8_t *p /* borrowed */);
void sr_ip_forward(struct sr_core* sr, uint8_t* packet /* given */);

/*-----------------------------------------------------------------------------
 * Component: ip_route(..) 
 *---------------------------------------------------------------------------*/

void sr_ip_route(struct sr_core* sr, uint8_t* packet /* given */);
uint32_t sr_find_srcip(struct sr_core* sr /* borrowed */, 
                       uint32_t ip_nbo);

#endif /* -- SR_IP_H -- */
