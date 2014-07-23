/*-----------------------------------------------------------------------------
 * File: arp_cache.h 
 * Date: Friday February 09, 2007 
 * Authors: Tom Deane
 * Contact: tdeane@stanford.edu
 *
 *---------------------------------------------------------------------------*/
 

#include "arp_cache.h"
#include "sr_router.h"

#include <string.h>
#include <assert.h>

/* 
 *--------------------------------------------------------------------- 
 * Method: init_arp_cache(struct sr_instance* sr)
 * Scope:  Global
 * 
 * Initializes the ARP cache linked list.
 * 
 *---------------------------------------------------------------------*/
void init_arp_cache(struct sr_instance* sr)
{
	Debug("Initializing cache\n");
	sr->cache_head = (struct cache_entry*)malloc(sizeof(struct cache_entry));
	sr->cache_tail = sr->cache_head;
	memset(sr->cache_head, 0, sizeof(struct cache_entry));
}


/* 
 *--------------------------------------------------------------------- 
 * Method: add_cache_entry(struct sr_instance* sr, uint32_t ip, 
 * 						   char *interface, uint8_t *ether_addr)
 * Scope:  Global
 * Checks to see if the entry is already in the cache, and if so,
 *  updates the entry.  Otherwise it adds a new mapping ebtween the IP
 * and the MAC address.
 * 
 * 
 *---------------------------------------------------------------------*/
void add_cache_entry(struct sr_instance* sr, uint32_t ip, char *interface, uint8_t *ether_addr)
{
	struct cache_entry* curr = sr->cache_head->next;
	while(curr){
		if (curr->ip==ip && strcmp(curr->interface, interface)==0){
			/* Found Entry -- Update it and reset timer */
			memcpy(curr->ether_addr, ether_addr, ETHER_ADDR_LEN);
			curr->start_time = time(NULL);
			return;
		}
		curr = curr->next;	
	}
	struct cache_entry* new_entry;
	new_entry = (struct cache_entry*)malloc(sizeof(struct cache_entry));
	new_entry->ip = ip;
	new_entry->next = NULL;
	memcpy(new_entry->ether_addr, ether_addr, ETHER_ADDR_LEN);
	Debug("Adding Cache Entry at time %ld for IP: ", time(NULL));
	print_ip(ip);
	new_entry->start_time = time(NULL); /*RESET TIMER */
	new_entry->interface = (char *)malloc(strlen(interface) + 1);
	strcpy(new_entry->interface, interface);
	sr->cache_tail->next = new_entry;
	sr->cache_tail = new_entry;
}

/* 
 *--------------------------------------------------------------------- 
 * Method: find_cache_entry(struct sr_instance* sr, uint32_t ip, char* interface, 
 * 					        uint8_t* ether_addr)
 * Scope:  Global
 * 
 * This function searches the cache for an entry.  If the entry is found, it returns
 * true and copies the result into memory pointed by ether_addr.The function relies
 * on the caller allocating space for ether_addr.
 * While searching for the entry, this piece of code invalidates (and frees) any
 * entries that have timed out.  I found it a cute little optimization to do this
 * here while we are searching the cache. 
 * 
 *---------------------------------------------------------------------*/
#define TIMEOUT_VAL 15
int find_cache_entry(struct sr_instance* sr, uint32_t ip, char* interface, 
				    uint8_t* ether_addr)
{
	struct cache_entry* curr = sr->cache_head->next;
	struct cache_entry* prev = sr->cache_head;
	while (curr){
		time_t elapsed = time(NULL) - curr->start_time;
		/* Time has elapsed, Erase cache entry before we even
		 * check it*/
		if (elapsed>TIMEOUT_VAL){
			Debug("Deleting Stale Cache Entry after %ld for IP: ", elapsed);
			print_ip(curr->ip);
			if (sr->cache_tail ==curr) {
				sr->cache_tail = prev;
			}
			free(curr->interface);
			prev->next = curr->next;
			free(curr);
			curr = prev->next;
		} else if (curr->ip==ip && strcmp(curr->interface, interface)==0){
			Debug("Found entry in cache with elapsed %ld for IP: ", elapsed);
			print_ip(curr->ip);
			memcpy(ether_addr, curr->ether_addr, ETHER_ADDR_LEN);
			return 1;		
		}
		else {
			curr = curr->next;
			prev = prev->next;
		}
	}
	/* Didn't find anything */
	return -1;
}

