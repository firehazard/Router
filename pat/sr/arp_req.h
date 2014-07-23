/*-----------------------------------------------------------------------------
 * File: arp_req.h 
 * Date: Friday February 09, 2007 
 * Authors: Tom Deane
 * Contact: tdeane@stanford.edu
 *
 *---------------------------------------------------------------------------*/
 
#ifndef ARP_REQ_H_
#define ARP_REQ_H_

#include "sr_router.h"

/*
 * The pending queue consists of a linked list of linked lists.
 * The first linked list is the ones for pending IP addresses
 * that we need a MAC address for.  Then, we have a secondary link
 * list for packets waiting for a particular IP in the queue.
 * 
 */

/* Link list entry for a secondary linked list (packets) */
struct packet_entry{
	uint8_t* packet;
	unsigned int len;
	struct packet_entry* next;	
};

/* Link list entry for main linked list (IPs) */
struct pending_ip {
	uint32_t ip_gw;
	char* interface;
	time_t start_time;
	unsigned short num_requests;
	struct packet_entry* packets_head;
	struct packet_entry* packets_tail;
	struct pending_ip* next;
};

void init_pending_arps(struct sr_instance* sr);

/* Call when you get ARP Reply*/
void queue_dispatch(struct sr_instance* sr, uint32_t ip, char* interface, uint8_t* ether_addr);

/* Call when you don't find packet in Cache*/
void queue_packet(struct sr_instance* sr, uint32_t ip_gw, unsigned int len, uint8_t* packet, 
				  char* interface);



#endif /*ARP_REQ_H_*/
