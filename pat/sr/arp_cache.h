/*-----------------------------------------------------------------------------
 * File: arp_cache.h 
 * Date: Friday February 09, 2007 
 * Authors: Tom Deane
 * Contact: tdeane@stanford.edu
 *
 *---------------------------------------------------------------------------*/

#ifndef ARP_CACHE_H_
#define ARP_CACHE_H_

/*Forward Declare */
struct sr_instance;

#include "sr_protocol.h"
#include "sr_router.h"

#include <sys/time.h>
#include <time.h>
#include <stdlib.h>


/* Data structure for ARP cache entries */
struct cache_entry {
	struct cache_entry* next;
	uint32_t ip;
	char* interface;
	uint8_t ether_addr[ETHER_ADDR_LEN];
	time_t start_time;	
};


void init_arp_cache(struct sr_instance* sr);

int find_cache_entry(struct sr_instance* sr, uint32_t ip, 
					  char* interface, uint8_t *ether_addr);

void add_cache_entry(struct sr_instance* sr, uint32_t ip,
					 char *interface, uint8_t *dst_ether_addr);



#endif /*ARP_CACHE_H_*/
