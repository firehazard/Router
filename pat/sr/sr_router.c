/**********************************************************************
 * file:  sr_router.c 
 * date: Friday February 09, 2007  
 * Contact: tdeane@stanford.edu
 *
 * Description:
 * 
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "string.h"
#include "arp_cache.h"
#include "arp_req.h"


/*--------------------------------------------------------------------- 
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 * 
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr) 
{
    /* REQUIRES */
    assert(sr);
    
    /* Add initialization code here! */
	init_arp_cache(sr);
	init_pending_arps(sr);
	
} /* -- sr_init -- */



/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/
void sr_handlepacket(struct sr_instance* sr, 
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
    /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);

    Debug("*** -> Received packet of length %d \n",len);

	/* The fun begins here.  We look at the ethernet header
	 * and check if it's an ARP packet or an IP packet.  Then
	 * we call the appropriate function */
	
	struct sr_ethernet_hdr* e_hdr = (struct sr_ethernet_hdr*)packet;
	if (e_hdr->ether_type == htons(ETHERTYPE_ARP)){
		handle_arp_packet(sr, packet, len, interface);
	}
	else if (e_hdr->ether_type == htons(ETHERTYPE_IP)) {
		handle_ip_packet(sr, packet, len, interface);
	}
	else {
		Debug("Invalid Packet Type. Droping Packet\n");
	}
}/* end sr_ForwardPacket */


/*---------------------------------------------------------------------
 * Method: handle_ip_packet(struct sr_instance* sr, uint8_t* packet, 
 * 							 unsigned int len, char* interface)
 * Scope:  Private
 * This method is called whenever we receive an IP packet.  Here is the
 * pseudo-code for this function
 * 	1.) Check IP checksum -- If it's invalid, drop the packet
 *  2.) Check if the packet has one of the router's IP as the destination IP
 * 		2.1) If this is the case, check if the packet is an ICMP message
 * 			2.1.1) If it is, confirm the ICMP checksum and call a helper function	
 * 				   for handling ICMP packets destined to the router
 *  3.) If the packet was destined to me but it's not an ICMP, we drop it like it's hot
 * 		and send back the port unreachable ICMP
 * 4.) If the packet was NOT destined to me, we call a helper fowarding function
 * 
 *---------------------------------------------------------------------*/
void handle_ip_packet(struct sr_instance* sr, uint8_t* packet, 
					  unsigned int len, char* interface)
{
   	 struct ip* ip_hdr = (struct ip*)(packet + sizeof(struct sr_ethernet_hdr));
 	 uint16_t original_sum = ip_hdr->ip_sum;
	 calc_checksum(ip_hdr, NULL, sizeof(struct ip));
   	 if (ip_hdr->ip_sum!=original_sum) {
	 	Debug("PACKET HAS BAD IP CHECKSUM. DROP!!\n");	
	 } else {
		 if (is_router_ip(sr, ip_hdr->ip_dst.s_addr)) {
		 	/* IP Packet Addressed to Me! */
		 	 if (ip_hdr->ip_p == IPPROTO_ICMP){
	 	 		struct icmp_header* icmp_hdr = (struct icmp_header*)(packet 
	 	 				 + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
	 	 		uint16_t original_checksum = icmp_hdr->checksum;
	 	 		size_t data_size = len - sizeof(struct sr_ethernet_hdr) - sizeof(struct ip);
				calc_checksum(NULL, icmp_hdr, data_size);
				if (icmp_hdr->checksum!=original_checksum){
					Debug("GOT BAD ICMP MESSAGE CHECKSUM. DROP\n");
				} else {
					/* Potential for ECHO request */
	 	 			sr_handle_icmp_echo(sr, packet, len, interface, icmp_hdr);
				}
		 	 }
		 	 else {
		 	 	Debug("Received non ICMP packet destined to router. Reply with ICMP \n");
		 	 	send_icmp_message(sr, packet, interface, len, 
		 	 				     DEST_UNREACHABLE,  PORT_UNREACHABLE, 1);
		 	 }
		 }
		 else {
		 	/* Packet not addressed to me - Let's forward it !*/
			handle_ip_forwarding(sr, packet, len, interface, ip_hdr);
		 }
   	 }
}

/*---------------------------------------------------------------------
 * Method: handle_ip_fowarding(struct sr_instance* sr, uint8_t* packet, 
					   unsigned int len, char* interface, struct ip* ip_hdr)
 * Scope:  Private
 *  This method is called when we are forwarding an IP packet.  First thing
 * we need to check is that the TTL is greater than 1. 
 *  We then need to find the next hop. If we can't find it, we send a net
 *  unreachable message.
 * If we find it, then we check to see if the next hop's gateway's ethernet
 * address is in our ARP cache. If so, we call yet another helper function to
 * forward the packet.
 * If we can't find it in the cache, we invoke the help of our arp requests 
 * queue to take care of it for us.... sweet!
 *  
 * ---------------------------------------------------------------------*/
void handle_ip_forwarding(struct sr_instance* sr, uint8_t* packet, 
					   unsigned int len, char* interface, struct ip* ip_hdr)
{
	if (ip_hdr->ip_ttl<2) {
 		send_icmp_message(sr, packet, interface, len, TIME_EXCEEDED, 0, 0);	
 	}
 	else {
		/* Packet has good TTL*/
		uint8_t dst_ether_addr[ETHER_ADDR_LEN];
		uint32_t ip_gw;
		char *dest_interface = find_next_hop(sr, ip_hdr->ip_dst.s_addr, &ip_gw);
		if (dest_interface==NULL) {
			/* Could not find next hop in the Routing Table, send ICMP*/
			send_icmp_message(sr, packet, interface, len, DEST_UNREACHABLE,NET_UNREACHABLE, 1);						
		} else {
			if (find_cache_entry(sr,ip_gw, dest_interface, dst_ether_addr)==1){
				Debug("IP In ARP Cache Routing For: ");
				print_ip(ip_hdr->ip_dst.s_addr);
				forward_packet(sr, dest_interface, dst_ether_addr, packet, len);
			}
			else {
				Debug("IP NOT in ARP Cache- Queue it up: ");
				print_ip(ip_hdr->ip_dst.s_addr);
				queue_packet(sr, ip_gw, len, packet, dest_interface);
			
			}
		}
 	}
}


/*---------------------------------------------------------------------
 * Method: find_next_hop(struct sr_instance* sr, uint32_t ip_dst, uint32_t* ip_gw)
 * Scope:  Private
 * Longest Prefix Match algorithm for finding the next hop gateway.  These are
 * the conditions we need to sastify to declare an entry in the RT to be the
 * next hop:
 * 	 1.)  The bit-wise 'AND' of the destination IP with the mask has to be equal
 *        to the bit-wise 'AND' of the RT's entry IP with the same mask.  If this
 * 		   the case, then we have a match in the RT
 *   2.) The mask has to be the longest mask we've found so far.  To find this, all
 * 	     we need to ensure is that the number of '1' bits in the longest mask so far
 * 	     is monotonically increasing.  We can do this by doing a bitwise 'AND' of the
 *       potential mask with the longest mask so far.  If this value is equal to the
 *      longest mask so far, then we know the potential mask had at least as many '1's
 * 	    as the longest mask so far.  Note that this holds regardless of the byte-order
 * 	    we are working with =)   
 * ---------------------------------------------------------------------*/
char* find_next_hop(struct sr_instance* sr, uint32_t ip_dst, uint32_t* ip_gw)
{	
	struct sr_rt* walker = sr->routing_table;
	unsigned int longest_mask = 0;
	char* result = NULL;
	while (walker) {
		/* If-statement condition Explained Above*/
		if (((ip_dst & walker->mask.s_addr) == 
			(walker->dest.s_addr & walker->mask.s_addr)) &&
			( (walker->mask.s_addr & longest_mask) == longest_mask)  ) {
				*ip_gw = walker->gw.s_addr;
				result = walker->interface;
				longest_mask = walker->mask.s_addr;
			} 
		walker = walker->next;	
	}
	/*Returns NULL if we didn't find anything*/
	return result;
}

/*---------------------------------------------------------------------
 * Method: forward_packet(struct sr_instance* sr, char* dst_interface, uint8_t* dst_ether_addr, 
				uint8_t* src_packet,  unsigned int len)
 * Scope:  Private
 * If we've made it this far, it means that we are ready to forward the packet out of appropriate
 * interface.  This function simply appends the correct ethernet headers to the packet,
 * decreases the TTL, recalculates the IP checksum, and finally sends the packet.
 * Note that this function can be called immediately after a packet arrives and it's next hop MAC
 * address is in the cache OR from the queue of pending packets after we get the corresponding
 *  ARP reply.
 * ---------------------------------------------------------------------*/
void forward_packet(struct sr_instance* sr, char* dst_interface, uint8_t* dst_ether_addr, 
				uint8_t* src_packet,  unsigned int len)
{
	struct sr_if* itf =  sr_get_interface(sr, dst_interface);
	uint8_t *outgoing_packet = (uint8_t*)malloc((size_t)len);
	/* create a copy of the packet so that we don't overwrite info
	 * we might need */
	memcpy(outgoing_packet, src_packet, len);
	
	/* Write new ethernet headers */
	struct sr_ethernet_hdr* e_hdr = (struct sr_ethernet_hdr*)outgoing_packet;
	struct ip* ip_hdr = (struct ip*)(outgoing_packet + sizeof(struct sr_ethernet_hdr));
	memcpy(e_hdr->ether_dhost, dst_ether_addr, ETHER_ADDR_LEN);
	memcpy(e_hdr->ether_shost, itf->addr , ETHER_ADDR_LEN);
	
	/* Decrease TTL, calculate new checksum, and send*/
	ip_hdr->ip_ttl--;
	calc_checksum(ip_hdr, NULL, sizeof(struct ip));
	sr_send_packet(sr, outgoing_packet, len, dst_interface);
		
	/* Free the packet copy */
	free(outgoing_packet);
}




/*---------------------------------------------------------------------
 * Method: handle_arp_packet(struct sr_instance* sr, uint8_t* packet, unsigned int len, char* interface)
 * Scope:  Private
 * We call this function whenever we get an ARP packet that is destined to the router.
 * Note that according to the RFC, we should cache the IP/MAC combo REGARDLESS of whether
 * we get an ARP request or reply.  This makes sense: why not use the data if we have it anyway?
 * If we get an ARP reply, we need to check the queue and dispatch any packets that were waiting
 * for that IP. 
 * ---------------------------------------------------------------------*/

void handle_arp_packet(struct sr_instance* sr, uint8_t* packet, unsigned int len, char* interface)
{
	struct sr_arphdr* a_hdr = (struct sr_arphdr*)(packet + sizeof(struct sr_ethernet_hdr));
	add_cache_entry(sr, a_hdr->ar_sip, interface, a_hdr->ar_sha);
    if (a_hdr->ar_op==htons(ARP_REQUEST)) {
    	/* Call helper function to handle arp request*/
    	send_arp_reply(sr, packet, len,  interface);
    }
	else if (a_hdr->ar_op==htons(ARP_REPLY)) {
		/* We got an ARP Reply, tell the pending queue about it */
		Debug("Got ARP Reply from IP: ");
		print_ip(a_hdr->ar_sip);
		queue_dispatch(sr, a_hdr->ar_sip, interface, a_hdr->ar_sha);						
	}
	else {
		/* We got an ARP packet with an unacceptable opcode*/
		Debug("Invalid ARP OPCODE in Ethernet Msg \n");				
	}
}



/*--------------------------------------------------------------------- 
 * Method: send_arp_reply
 * Scope: Private
 * This function simply sends an ARP reply whenever we get a valid
 * ARP request.  Details below
 *---------------------------------------------------------------------*/
 
void send_arp_reply(struct sr_instance* sr, uint8_t * packet,
										 unsigned int len,  char* interface)
 {
 	Debug("Received an ARP request from interface: %s\n", interface);
 	struct sr_if* itf =  sr_get_interface(sr, interface);
	/* Make packet copy so we don't overwrite useful data */
	uint8_t *reply_packet = (uint8_t*)malloc((size_t)len);
	memcpy(reply_packet, packet, len);
		
	/* Get pointers to key locations */
	struct sr_ethernet_hdr* reply_e_hdr = (struct sr_ethernet_hdr*)reply_packet;
	struct sr_arphdr* reply_a_hdr = (struct sr_arphdr*)(reply_packet 
											 + sizeof(struct sr_ethernet_hdr));
	struct sr_ethernet_hdr* req_e_hdr = (struct sr_ethernet_hdr*)packet;
	struct sr_arphdr* req_a_hdr = (struct sr_arphdr*)(packet 
											 + sizeof(struct sr_ethernet_hdr));
	 
	/* Swap Ethernet and ARP header source/destination addresses*/
	 memcpy(reply_e_hdr->ether_dhost, req_e_hdr->ether_shost, ETHER_ADDR_LEN);
	 memcpy(reply_a_hdr->ar_tha, req_e_hdr->ether_shost, ETHER_ADDR_LEN);
	 memcpy(reply_e_hdr->ether_shost, itf->addr, ETHER_ADDR_LEN);
	 memcpy(reply_a_hdr->ar_sha, itf->addr,ETHER_ADDR_LEN);
	 
	 /* Set appropriate header values */
	 reply_a_hdr->ar_sip = itf->ip;
	 reply_a_hdr->ar_tip = req_a_hdr->ar_sip;
	 reply_a_hdr->ar_op = htons(ARP_REPLY);
	 
	 /*Send packet and set free*/
	 sr_send_packet(sr, reply_packet, len, interface);
	 free(reply_packet);
 }


/********************* ICMP FUNCTIONS ***************************************/


/*--------------------------------------------------------------------- 
 * Method: sr_handle_icmp_echo(struct sr_instance* sr, uint8_t* packet,
							   unsigned int len, char* interface, struct icmp_header* icmp_hdr)
 * Scope: Private
 * I found it a lot cleaner to deal with ECHO replies separately from other ICMP message  simply
 * because ECHO messages have many things that only apply to it:  for example, you normally take
 * only 64 bits of data from the original datagram for any ICMP message except ECHO (where you
 * replicate ALL the data).  Addresses are just swapped for echo whereas it's bit more complicated
 * for other ICMP messages, etc. Details below...
 * 
 *---------------------------------------------------------------------*/
void sr_handle_icmp_echo(struct sr_instance* sr, uint8_t* packet,
							   unsigned int len, char* interface, struct icmp_header* icmp_hdr)
{
	if (icmp_hdr->type == ICMP_TYPE_ECHO) {
		Debug("Received an ICMP Echo Request\n");
		struct sr_if* itf =  sr_get_interface(sr, interface);
		uint8_t *reply_packet = (uint8_t*)malloc((size_t)len);
		memcpy(reply_packet, packet, len);
		/* Swap Ethernet addresses */
		struct sr_ethernet_hdr* reply_e_hdr = (struct sr_ethernet_hdr*)reply_packet;
		struct sr_ethernet_hdr* req_e_hdr = (struct sr_ethernet_hdr*)packet;
		memcpy(reply_e_hdr->ether_dhost, req_e_hdr->ether_shost, ETHER_ADDR_LEN);
		memcpy(reply_e_hdr->ether_shost, itf->addr, ETHER_ADDR_LEN);
 		/* Swap IP addresses*/
 		struct ip* reply_ip_hdr = (struct ip*)(reply_packet + sizeof(struct sr_ethernet_hdr));
 		struct ip* req_ip_hdr = (struct ip*)(packet + sizeof(struct sr_ethernet_hdr));
 		reply_ip_hdr->ip_dst = req_ip_hdr->ip_src;
  		reply_ip_hdr->ip_src = req_ip_hdr->ip_dst;
 		/* Reset TTL and calculate IP checksum */
 		reply_ip_hdr->ip_ttl = INIT_TTL ;
 		calc_checksum(reply_ip_hdr, NULL,  sizeof(struct ip));
 		/* Change message header to ECHO REPLY, calculate ICMP checksum and send */
 		struct icmp_header* icmp_hdr_reply = (struct icmp_header*)
 						(reply_packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
 		icmp_hdr_reply->type = ICMP_TYPE_ECHO_REPLY;
 		calc_checksum(NULL, icmp_hdr, len - sizeof(struct sr_ethernet_hdr) - sizeof(struct ip));
 		sr_send_packet(sr, reply_packet, len, interface);	  
 		free(reply_packet);	  
	}
	else {
		Debug("Received a non-echo ICMP Message. Drop Packet\n");
	}
}



/*--------------------------------------------------------------------- 
 * Method: send_icmp_message(struct sr_instance* sr, uint8_t* trouble_packet, char* interface, 
					   unsigned int len,  uint8_t type, uint8_t code, int use_dest_ip)
 * Scope: Private
 * We use this function for any type of ICMP message we wish to send with the exception of
 * ECHO ICMP ( reason explained in comment above).  The packet is prepared exactly as specified
 * by the ICMP RFC.
 *---------------------------------------------------------------------*/
void send_icmp_message(struct sr_instance* sr, uint8_t* trouble_packet, char* interface, 
					   unsigned int len,  uint8_t type, uint8_t code, int use_dest_ip)
{
	size_t header_size = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
	size_t icmp_len = header_size + sizeof(struct icmp_header);
	uint8_t *icmp_packet = (uint8_t*)malloc((size_t)icmp_len);
	memcpy(icmp_packet, trouble_packet, header_size);
	/* Prepare Ethernet and IP Headers*/
	prepare_icmp_headers(trouble_packet, icmp_packet, sr_get_interface(sr, interface), use_dest_ip);
	/* Now get the ICMP header */
	struct icmp_header* icmp_hdr = (struct icmp_header*)(icmp_packet + header_size);
	/* Copy the IP Header into the ICMP header (as defined by the RFC)*/
	memcpy(&icmp_hdr->ip_hdr, trouble_packet+sizeof(struct sr_ethernet_hdr), sizeof(struct ip));
	size_t data_size = len - header_size;
	
	if (data_size>=ICMP_DATA_LEN){
		/* The packet that we received has more than 64 bits of data, take it all */
		memcpy(icmp_hdr->data, trouble_packet+header_size, ICMP_DATA_LEN);
	} else {
		/* The packet that we received has LESS than 64 bits of data, just pad with 0s*/
		memcpy(icmp_hdr->data, trouble_packet+header_size, data_size);
		memset(icmp_hdr->data + data_size, 0, ICMP_DATA_LEN-data_size);
	}
	/* Set TYPE and CODE*/
	icmp_hdr->unused = 0;
	icmp_hdr->type = type;
	icmp_hdr->code = code;
	/* Calculate checksum and send packet */
	calc_checksum(NULL, icmp_hdr, sizeof(struct icmp_header));	
	sr_send_packet(sr, icmp_packet, icmp_len, interface);
	free(icmp_packet);
}

/*--------------------------------------------------------------------- 
 * Method: prepare_icmp_headers(uint8_t* trouble_packet, uint8_t* icmp_packet, 
 * 								struct sr_if* itf, int use_dest_ip))
 * Scope: Private
 *  Helper function for setting ethernet headers, setting the correct IP addresses
 * of the outgoing packet as well as other IP header fields.  It is called by 
 * send_icmp_message from above.
 *---------------------------------------------------------------------*/
void prepare_icmp_headers(uint8_t* trouble_packet, uint8_t* icmp_packet,
						  struct sr_if* itf, int use_dest_ip)
{
	/* Format Ethernet header */
	struct ip* trouble_ip_hdr = (struct ip*)(trouble_packet + sizeof(struct sr_ethernet_hdr));
	struct ip* icmp_ip_hdr = (struct ip*)(icmp_packet + sizeof(struct sr_ethernet_hdr));
	struct sr_ethernet_hdr* icmp_e_hdr = (struct sr_ethernet_hdr*)icmp_packet;
	struct sr_ethernet_hdr* trouble_e_hdr = (struct sr_ethernet_hdr*)trouble_packet;	
	memcpy(icmp_e_hdr->ether_dhost, trouble_e_hdr->ether_shost, ETHER_ADDR_LEN);
	memcpy(icmp_e_hdr->ether_shost, itf->addr, ETHER_ADDR_LEN);
	
	/* Format IP Header*/
	icmp_ip_hdr->ip_dst = trouble_ip_hdr->ip_src;
    if (use_dest_ip==0){
    	/* TTL ICMP or HOST unreachable*/
		icmp_ip_hdr->ip_src.s_addr = itf->ip;
	 }
	else  {
		/* Port unreachable*/
		icmp_ip_hdr->ip_src = trouble_ip_hdr->ip_dst;
	 }
	icmp_ip_hdr->ip_len = htons(sizeof(struct ip) + sizeof(struct icmp_header));
	icmp_ip_hdr->ip_ttl = INIT_TTL;
	icmp_ip_hdr->ip_p = IPPROTO_ICMP;
	/* Recaculate checksum and send */
	calc_checksum(icmp_ip_hdr, NULL, sizeof(struct ip));
	Debug("Sending ICMP msg for IP:");
	print_ip(icmp_ip_hdr->ip_dst.s_addr);
}


/*--------------------------------------------------------------------- 
 * Method: calc_checksum(struct ip* ip_hdr, struct icmp_header* icmp_hdr, size_t data_length)
 * Scope: Private
 * This function calculates the checksum for both IP and ICMP headers.  If you want to calculate
 * the checksum for an ICMP packet, you put NULL for the IP header (and viceversa).  
 * The checksum is just the 16-bit one's complement sum of consecutive 16 bytes in the header (and data 
 * for ICMP).  Edge cases are taken care of.
 *---------------------------------------------------------------------*/
void calc_checksum(struct ip* ip_hdr, struct icmp_header* icmp_hdr, size_t data_length)
{
	unsigned int sum = 0;
	int i;
	uint16_t* ptr = NULL;
	/* Set current checksum to 0*/
	if (icmp_hdr==NULL){
		ip_hdr->ip_sum = 0;
		ptr = (uint16_t *)ip_hdr;
	} else {
		icmp_hdr->checksum = 0;
		ptr = (uint16_t *)icmp_hdr;
	}
	/*Do 16-bit sums*/
	size_t size = data_length/2;
	for (i=0;i<size; i++) {
		sum+= *(ptr+i);
	}
	/* Check if data length was odd, and if so
	 * we need to add another 8 bits*/
	if ((data_length%2) != 0) {
		sum+= *(uint8_t *)((char *)ptr + size);
	}
	/* Magic from Steven's book that's confusing*/
	sum = (sum>>16) + (sum & 0xffff);
	sum+= (sum>>16);
	if (icmp_hdr==NULL){
		ip_hdr->ip_sum =  ~sum;
	}
	else {
		icmp_hdr->checksum =  ~sum;
	}
}

/*--------------------------------------------------------------------- 
 * Method: is_router_ip(struct sr_instance* sr, uint32_t ip)
 * Scope: Private
 * Tells us if a particular IP address corresponds to one of the router's
 * interfaces
 *---------------------------------------------------------------------*/
int is_router_ip(struct sr_instance* sr, uint32_t ip)
{
	struct sr_rt* walker = sr->routing_table;
	while(walker){
		if (sr_get_interface(sr, walker->interface)->ip == ip){
			return 1;	
		} 
		walker = walker->next;	
	}
	return 0;
}

/*--------------------------------------------------------------------- 
 * Method: print_ip(uint32_t ip)
 * Scope: Private
 * Silly little convenience function for printing an IP address.
 *---------------------------------------------------------------------*/
void print_ip(uint32_t ip)
{
	Debug("%s\n", inet_ntoa(*(struct in_addr *)&ip)); 		
} 
 
 
 
