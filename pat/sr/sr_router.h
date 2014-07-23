/*-----------------------------------------------------------------------------
 * File: sr_router.h 
 * Date: ?
 * Authors: Guido Apenzeller, Martin Casado, Virkam V, Tom Deane.
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_ROUTER_H 
#define SR_ROUTER_H

#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>

#include "sr_protocol.h"

#include "arp_cache.h"
#include "arp_req.h"
 
/* we dont like this debug , but what to do for varargs ? */
#ifdef _DEBUG_
#define Debug(x, args...) printf(x, ## args)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) printf("%02x:", \
  (unsigned char)(x[ivyl])); printf("%02x",(unsigned char)(x[5])); } while (0) 
#else
#define Debug(x, args...) do{}while(0)
#define DebugMAC(x) do{}while(0)
#endif

#define INIT_TTL 64 
#define PACKET_DUMP_SIZE 1024 
#define MIN_PACKET_LENGTH 60

/* forward declare */
struct sr_if;
struct sr_rt;


/* ----------------------------------------------------------------------------
 * struct sr_instance
 *
 * Encapsulation of the state for a single virtual router. 
 *
 * -------------------------------------------------------------------------- */

struct sr_instance
{
    int  sockfd;   /* socket to server */
    char user[32]; /* user name */
    char host[32]; /* host name */ 
    unsigned short topo_id;
    struct sockaddr_in sr_addr; /* address to server */
    struct sr_if* if_list; /* list of interfaces */
    struct sr_rt* routing_table; /* routing table */
    
    struct pending_ip* queue_head;
    struct pending_ip* queue_tail;
    pthread_mutex_t queue_lock;
    pthread_t arp_daemon;
    
    struct cache_entry* cache_head;
    struct cache_entry* cache_tail;
   
    
    
    FILE* logfile;
};

#define ICMP_TYPE_ECHO 8
#define ICMP_TYPE_ECHO_REPLY 0


#define DEST_UNREACHABLE 3
#define NET_UNREACHABLE 0
#define HOST_UNREACHABLE 1
#define PORT_UNREACHABLE 3
#define HOST_UNREACHABLE 1
#define TIME_EXCEEDED  11


#define ICMP_DATA_LEN 8
struct icmp_header
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint32_t unused;
	struct ip ip_hdr;
	uint8_t data[ICMP_DATA_LEN];
};


/* -- sr_main.c -- */
int sr_verify_routing_table(struct sr_instance* sr);

/* -- sr_vns_comm.c -- */
int sr_send_packet(struct sr_instance* , uint8_t* , unsigned int , const char*);
int sr_connect_to_server(struct sr_instance* ,unsigned short , char* );
int sr_read_from_server(struct sr_instance* );

/* -- sr_router.c -- */
void sr_init(struct sr_instance* );
void handle_ip_packet(struct sr_instance* sr, uint8_t* packet, 
					  unsigned int len, char* interface);
void handle_arp_packet(struct sr_instance* sr, uint8_t* packet, 
						unsigned int len, char* interface);
void handle_ip_forwarding(struct sr_instance* sr, uint8_t* packet, 
					   unsigned int len, char* interface, struct ip* ip_hdr);
void sr_handlepacket(struct sr_instance* , uint8_t * , unsigned int , char* );
void send_arp_reply(struct sr_instance* sr, uint8_t * packet,
										  unsigned int len, char* interface);
void sr_handle_icmp_echo(struct sr_instance* sr, uint8_t * packet,
										  unsigned int len, char* interface, struct icmp_header* icmp_hdr);
void calc_checksum(struct ip* ip_hdr, struct icmp_header* icmp_hdr, size_t data_length);
void forward_packet(struct sr_instance* sr, char* dst_interface, 
					uint8_t* dst_ether_addr, uint8_t* src_packet, unsigned int len);						  										  
char* find_next_hop(struct sr_instance* sr, uint32_t ip_dst, uint32_t* ip_gw);
void print_ip(uint32_t ip);
int check_checksum(struct ip* ip_hdr);
void send_icmp_message(struct sr_instance* sr, uint8_t* trouble_packet, char* interface, 
					  unsigned int len, uint8_t type, uint8_t code, int use_dest_ip);
int is_router_ip(struct sr_instance* sr, uint32_t ip);
void prepare_icmp_headers(uint8_t* trouble_packet, uint8_t* icmp_packet, struct sr_if* itf, int use_dest_ip);


/* -- sr_if.c -- */
void sr_add_interface(struct sr_instance* , const char* );
void sr_set_ether_ip(struct sr_instance* , uint32_t );
void sr_set_ether_addr(struct sr_instance* , const unsigned char* );
void sr_print_if_list(struct sr_instance* );

#endif /* SR_ROUTER_H */
