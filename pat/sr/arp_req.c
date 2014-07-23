#include "arp_req.h"
#include "sr_if.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


/* Private Fnc Prototypes */
struct pending_ip* find_entry(struct sr_instance* sr, uint32_t ip, char *interface);
void send_arp_request(struct sr_instance* sr, uint32_t ip, char* interface);
void drop_pending_packets(struct sr_instance* sr, struct pending_ip* pip);
void retry_arp_request(struct sr_instance*sr, struct pending_ip* pip);
void check_queue_timeouts(struct sr_instance *);
void insert_new_ip(struct sr_instance* sr, struct packet_entry* new_entry,
				uint32_t ip_gw, char* interface);
void *arp_daemon_fnc(void *sr_temp);

/*--------------------------------------------------------------------- 
 * Method: init_pending_arps(struct sr_instance* sr)
 * Scope:  Global
 * Thread: Main Thread
 * 
 * Initializes the queue of pending arp requests and forks off a new
 * thread that will perform queue checks for TIMEOUTS (more below).
 * 
 *---------------------------------------------------------------------*/
void init_pending_arps(struct sr_instance* sr)
{
	Debug("Initializing Pending Queue\n");
	sr->queue_head = (struct pending_ip*)malloc(sizeof(struct pending_ip));
	memset(sr->queue_head,0,sizeof(struct pending_ip));
	sr->queue_tail = sr->queue_head;
	pthread_mutex_init(&sr->queue_lock, NULL);
	pthread_create(&sr->arp_daemon, NULL, arp_daemon_fnc, (void *)sr);
}


/*--------------------------------------------------------------------- 
 * Method: void* arp_daemon_fnc(void *sr_tem)
 * Scope:  Private
 * Thread: Child Thread
 * 
 *  This is the function performed by the only other thread in the router.
 * It runs continously (sleeping for 100ms between loops) during the life
 * of the program.
 * This daemon (and its helper fncs) are responsible for:
 *    A.) Checking if any ARP requests have timed out 
 *    B.) If so:
 * 	   - Check if we have reached the MAX number of requests
 *        - If not, then send another ARP request
 * 	  - If so, then drop all packets in queue and send corresponding ICMPs
 * 
 *---------------------------------------------------------------------*/
#define SLEEP_TIME 0.1
void *arp_daemon_fnc(void *sr_temp)
{
	struct sr_instance* sr = (struct sr_instance *)sr_temp;
	while (1){
		pthread_mutex_lock(&sr->queue_lock);
		check_queue_timeouts(sr);
		pthread_mutex_unlock(&sr->queue_lock);
		sleep(SLEEP_TIME);
	}
	return 0;
}

/*--------------------------------------------------------------------- 
 * Method: check_queue_timeouts(struct sr_instance* sr)
 * Scope:  Private
 * Thread: Child Thread
 * 
 *  Helper function ran by the child thread for checking arp requests
 * timeouts.
 * 
 *---------------------------------------------------------------------*/
#define ARP_TIME_OUT 1
#define MAX_ARP_REQUESTS 5
void check_queue_timeouts(struct sr_instance* sr)
{
	struct pending_ip* curr = sr->queue_head->next;		
	struct pending_ip* prev = sr->queue_head;
	while (curr!=NULL){
		time_t elapsed_time = time(NULL) - curr->start_time;
		if (elapsed_time>ARP_TIME_OUT){
			/* Time is up */
			if (curr->num_requests == MAX_ARP_REQUESTS) {
				/* Never got a reply, drop all packets waiting for that ip*/
				drop_pending_packets(sr, curr);						
				if (sr->queue_tail == curr) {
					sr->queue_tail = prev;
				}
				/* Updated link list pointers*/
				prev->next = curr->next;
				free(curr->packets_head);
				free(curr->interface);
				free(curr);
				curr = prev->next;	
			}
			else {
				/* Didn't get a reply after 1 second, try again*/
				retry_arp_request(sr, curr);
				curr = curr->next;
				prev = prev->next;	
			} 	
		}
		else {
	 		curr = curr->next;
			prev = prev->next;
		}
	}
}

/*--------------------------------------------------------------------- 
 * Method: drop_pending_packets(struct sr_instance* sr, struct pending_ip* pip)
 * Scope:  Private
 * Thread: Child Thread
 * 
 * If this function was called, it means that we sent 5 ARP Requests but never
 * got a single reply for it. So now we take ALL packets that were waiting
 * on that IP resolution, send ICMP messages back for all of them, and then
 * drop them. 
 * 
 *---------------------------------------------------------------------*/
void drop_pending_packets(struct sr_instance* sr, struct pending_ip* pip)
{
	Debug("NEVER got an ARP reply for IP: ");
	print_ip(pip->ip_gw);
	struct packet_entry* walker = pip->packets_head->next;
	while(walker!=NULL){
		send_icmp_message(sr, walker->packet, pip->interface, walker->len, 
									DEST_UNREACHABLE, HOST_UNREACHABLE, 0);
		free(walker->packet);
		struct packet_entry* victim = walker;
		walker = walker->next;
		free(victim);
	}	
}

/*--------------------------------------------------------------------- 
 * Method: retry_arp_request(struct sr_instance*sr, struct pending_ip* pip)
 * Scope:  Private
 * Thread: Child Thread
 * 
 * Helper function for performing a retry ARP Request.  We need to increment
 * the number of retries for that IP and reset the timer.  We finally call
 * a helper function to actually send the packet out.
 * 
 *---------------------------------------------------------------------*/
void retry_arp_request(struct sr_instance*sr, struct pending_ip* pip)
{
	/* Try Seding More ARP Request */
	Debug("ARP Reply TIMEOUT - Sending ANOTHER ARP request for IP: ");
	print_ip(pip->ip_gw);
	pip->num_requests++;
	pip->start_time = time(NULL);
	send_arp_request(sr, pip->ip_gw, pip->interface);
}


/*--------------------------------------------------------------------- 
 * Method: retry_arp_request(struct sr_instance*sr, struct pending_ip* pip)
 * Scope:  Public
 * Thread: Main Thread
 * 
 * We have received a packet for which we do not have an entry in the ARP cache
 * (for its outgoing gateway).  We need to insert the packet into our lists
 * of pending packets and send the first arp request.
 * 
 *---------------------------------------------------------------------*/
void queue_packet(struct sr_instance* sr, uint32_t ip_gw, 
							unsigned int len, uint8_t* packet, /*borrowed*/
							char* interface)
{
	pthread_mutex_lock(&sr->queue_lock);
	/* Create packet copy*/
	struct packet_entry* new_entry = 
					(struct packet_entry*)malloc(sizeof(struct packet_entry));
	new_entry->packet = (uint8_t *)malloc(len);
	memcpy(new_entry->packet, packet, len);
	new_entry->len = len;				
	new_entry->next = NULL;
	
	struct pending_ip* pos = find_entry(sr, ip_gw, interface);
	
	if (pos!=NULL) {
		/* We already have an IP entry for this, just queue the packet */
		Debug("Queing Packet Request through %s for IP: ", interface);
		print_ip(ip_gw);
		pos->packets_tail->next = new_entry;
		pos->packets_tail = new_entry;		
	}	
	else {
		/* This is a new IP, insert it in the queue and send ARP Request*/
		insert_new_ip(sr, new_entry, ip_gw, interface);
	}
	pthread_mutex_unlock(&sr->queue_lock);
}

/*--------------------------------------------------------------------- 
 * Method: void insert_new_ip(struct sr_instance* sr, struct packet_entry* new_entry,
					uint32_t ip_gw, char* interface)
 * Scope:  Private
 * Thread: Main Thread
 *  
 * We have a brand new IP in to be added to the queue.  Allocate an etry for it,
 * start the timer, and send an ARP request.
 * 
 *---------------------------------------------------------------------*/
void insert_new_ip(struct sr_instance* sr, struct packet_entry* new_entry,
					uint32_t ip_gw, char* interface)
{
	/* Packet Not in Cache and IP for it not in Queue*/
	/* Send ARP Request */
	Debug("Seding ARP Request through %s for IP: ", interface);
	print_ip(ip_gw);
	send_arp_request(sr, ip_gw, interface);
	
	/*Add entry to queue and Start the Timer */
	struct pending_ip* new_ip = 
		(struct pending_ip*)malloc(sizeof(struct pending_ip));
	new_ip->interface = (char *)malloc(strlen(interface) + 1);
	strcpy(new_ip->interface, interface);
	new_ip->start_time = time(NULL);
	new_ip->num_requests = 1;
	new_ip->packets_head = (struct packet_entry*)malloc(sizeof(struct packet_entry));
	memset(new_ip->packets_head, 0, sizeof(struct packet_entry));
	
	/* Attach incoming packet to the new queue entry (new_ip) */
	new_ip->packets_head->next = new_entry;
	new_ip->packets_tail = new_entry;
	new_ip->ip_gw = ip_gw;
	new_ip->next = NULL;
	sr->queue_tail->next = new_ip;
	sr->queue_tail = new_ip;
}

/*--------------------------------------------------------------------- 
 * Method: send_arp_request(struct sr_instance* sr, uint32_t ip_gw, char* interface)
 * Scope:  Private
 * Thread: Main Thread and Child Thread (lock protected)
 *  
 * Prepares an ARP request message for a certain IP gateway. This function gets
 * down to the dirty details of setting up packet headers
 *---------------------------------------------------------------------*/
void send_arp_request(struct sr_instance* sr, uint32_t ip_gw, char* interface)
{
	uint8_t* arp_packet = (uint8_t *)malloc(MIN_PACKET_LENGTH);
	/* Prepare Ethernet Header */
	struct sr_ethernet_hdr* e_hdr = (struct sr_ethernet_hdr*)arp_packet;
	struct sr_arphdr* a_hdr = 
		(struct sr_arphdr*)(arp_packet+sizeof(struct sr_ethernet_hdr));
	struct sr_if* iface = sr_get_interface(sr, interface);
	/* Destination Unknown: use 0xFFFFFF */
	memset(e_hdr->ether_dhost, 0xFF, ETHER_ADDR_LEN); 
	memcpy(e_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
	e_hdr->ether_type = htons(ETHERTYPE_ARP);
	/* Setup ARP Headers */
	a_hdr->ar_hrd = htons(ARPHDR_ETHER);
	a_hdr->ar_pro = htons(ETHERTYPE_IP);
	a_hdr->ar_hln = ETHER_ADDR_LEN;
	a_hdr->ar_pln = sizeof(ip_gw);
	a_hdr->ar_op = htons(ARP_REQUEST);
	memcpy(a_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
	memset(a_hdr->ar_tha, 0, ETHER_ADDR_LEN);
	a_hdr->ar_sip = iface->ip;
	a_hdr->ar_tip = ip_gw;
	/* Pad with 0s to reach minimum length */
	size_t bytes_so_far = sizeof(struct sr_ethernet_hdr) + 
								sizeof(struct sr_arphdr);
	size_t bytes_left = MIN_PACKET_LENGTH - bytes_so_far;
	if (bytes_left>0){
		memset(arp_packet+bytes_so_far, 0, bytes_left);	
	}
	/*Send the packet at last*/
	sr_send_packet(sr, arp_packet, MIN_PACKET_LENGTH, interface);
	free(arp_packet);	
}

/*--------------------------------------------------------------------- 
 * Method: queue_dispatch(struct sr_instance* sr, uint32_t ip,
 * 						  char* interface, uint8_t *dst_ether_addr)
 * Scope:  Public
 * Thread: Main Thread
 *   
 * We have just received an ARP reply.  Check to see if any packets were
 * waiting for that reply and if so, dispatch all those packets waiting for it.
 * 
 *---------------------------------------------------------------------*/
void queue_dispatch(struct sr_instance* sr, uint32_t ip, char* interface, 
				     uint8_t *dst_ether_addr)
{
	pthread_mutex_lock(&sr->queue_lock);
	/* Look for that IP in the queue */
	struct pending_ip* pos = find_entry(sr, ip, interface);
	Debug("Checking DISPATCH for %s for IP: ", interface);
	print_ip(ip);
	if (pos!=NULL){
		/*We just got a reply for something in the queue, dispatch*/	
		struct packet_entry* curr = pos->packets_head->next;
		/* Iterate through list of pending packets */
		while(curr!=NULL){
			Debug("Queue DISPATCH! - Ip:");
			print_ip(ip);
			/* Forward the packet now that we know where is going*/
			forward_packet(sr, interface, dst_ether_addr, curr->packet, curr->len);
			free(curr->packet);
			struct packet_entry* victim = curr;
			curr = curr->next;
			free(victim);
		}
		/* Remove the entry from the pending queue*/
		struct pending_ip* iter = sr->queue_head;
		/* I wish I had a doubly-linked list*/
		while (iter->next!=pos) iter=iter->next;
		iter->next = pos->next;
		if (pos == sr->queue_tail){
			sr->queue_tail = iter;	
		}
		free(pos->packets_head);
		free(pos->interface);
		free(pos);
	}
	pthread_mutex_unlock(&sr->queue_lock);
}


/*--------------------------------------------------------------------- 
 * Method: find_entry(struct sr_instance* sr, uint32_t ip, char *interface)
 * Scope:  Private
 * Thread: Main Thread
 *   
 * Helper function for locating an entry in the pending queue of IPs.
 * 
 *---------------------------------------------------------------------*/
struct pending_ip* find_entry(struct sr_instance* sr, uint32_t ip, char *interface)
{
	struct pending_ip* curr = sr->queue_head->next;		
	while (curr!=NULL){
		if (ip == curr->ip_gw && strcmp(interface, curr->interface)==0) {
			return curr;
		}
		curr = curr->next;
	}
	return NULL;
}
	
