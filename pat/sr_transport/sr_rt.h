/*-----------------------------------------------------------------------------
 * file:  sr_rt.h 
 * date:  Mon Oct 07 03:53:53 PDT 2002  
 * Author: casado@stanford.edu
 *
 * Description:
 *
 * Methods and datastructures for handeling the routing table
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_RT_H
#define SR_RT_H

#ifdef _DARWIN_
#include <sys/types.h>
#endif

#include <netinet/in.h>

#include "sr_router.h"
#include "sr_if.h"

/* ----------------------------------------------------------------------------
 * struct sr_rt
 *
 * Node in the routing table 
 *
 * -------------------------------------------------------------------------- */

struct sr_rt
{
    struct in_addr dest;
    struct in_addr gw;
    struct in_addr mask;
    char   interface[SR_IFACE_NAMELEN];
    struct sr_rt* next;
};


int sr_load_rt(struct sr_core*,const char*);
void sr_add_rt_entry(struct sr_core*, struct in_addr,struct in_addr,
                  struct in_addr,char*);
void sr_print_routing_table(struct sr_core* sr);
void sr_print_routing_entry(struct sr_rt* entry);

int  sr_verify_routing_table(struct sr_core* sr);


#endif  /* --  SR_RT_H -- */
