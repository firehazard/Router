/*-----------------------------------------------------------------------------

 * date:  Wed Nov 12 14:53:36 PST 2003 
 * Author: Martin Casado
 *
 * Description:
 *
 * Header file for threaded ARP subsystem.
 * The Arp subsystem is comprised of three main components:
 *
 * arp_input(..)  - accepts all arp packets
 * arp_output(..) - generates all arp packets and handles IP packets
 *                  that need to be arped for
 * arp_state(..)  - thread safe access to the arp cache and arp queue
 *         arp_cache : cached ip->mac address pairs taken from arp replies
 *                     or static entries from user space
 *         arp_queue : queue of IP packets waiting for pending ARP replies
 *
 * Supports:
 *
 *   - static arp caches
 *   - ARP request generation
 *   - ARP reply generation
 *   - queued IP packets waiting for ARP replies
 *   - ARP cache with timeouts
 *   - static ARP cache entries
 *   - multiple threads
 *
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_ARP_H
#define SR_ARP_H

#include <sys/time.h>
#include <pthread.h>
#if defined(_LINUX_)
#include <stdint.h>
#elif defined(_SOLARIS_)
#include <inttypes.h>
#endif

#include "sr_if.h"
#include "sr_protocol.h"
#include "sr_ethernet.h"


#define DEFAULT_ARP_CACHE_TIMEOUT 10   /* seconds */
#define DEFAULT_ARP_QUEUE_TIMEOUT 10   /* seconds */

struct sr_core; /* -- forward declaration -- */

/*-----------------------------------------------------------------------------
 * ARP cache entry 
 *---------------------------------------------------------------------------*/

struct arp_entry
{
    struct   timeval tv;
    uint32_t ip;                   /* host's IP address nbo */
    char     mac[ETHER_ADDR_LEN];  /* host's MAC address */
    struct   arp_entry *next;     
};

/*-----------------------------------------------------------------------------
 * Entry for packets waiting for pending ARPs 
 *---------------------------------------------------------------------------*/

typedef struct ip_entry
{
    uint8_t*  packet; /* given */
    uint32_t  next_hop_ip;
    char   interface[SR_IFACE_NAMELEN];
    struct timeval tv;
    struct ip_entry* next;
} ip_entry;

/*-----------------------------------------------------------------------------
 * Contains all data members of ARP subsystem 
 *---------------------------------------------------------------------------*/

typedef struct arp_state
{
    struct arp_entry arp_cache_head;
    struct ip_entry  arp_queue_head;

    pthread_t arp_worker_thread;
    pthread_mutex_t arp_cache_mutex;
    pthread_mutex_t arp_queue_mutex;
    volatile uint8_t thread_exit_condition;
    volatile uint8_t arp_cache_timeout;
    volatile uint8_t arp_queue_timeout;
} arp_state;

/*-----------------------------------------------------------------------------
 * Component: arp_state(..) 
 *---------------------------------------------------------------------------*/

void sr_arp_subsystem_startup (struct sr_core* );
void sr_arp_subsystem_shutdown(struct sr_core* );

int sr_get_arp_entry(struct sr_core* sr,
                     uint32_t ip /* nbo */,
                     char mac[ETHER_ADDR_LEN]);

int sr_clean_arp_cache(struct sr_core* );
int sr_clean_arp_queue(struct sr_core* );

void sr_prune_arp_cache_queue(struct sr_core* );

void sr_arp_cache_add_entry(struct sr_core* sr, 
                            struct arp_entry* entry, /* given */
                            uint8_t is_static);

void sr_arp_queue_add_entry(struct sr_core*, 
                            uint8_t* /* given */, 
                            uint32_t, 
                            char* );

int sr_delete_cache_entry(struct sr_core* sr, uint32_t );

void sr_print_arp_cache(struct sr_core* sr);
void sr_print_arp_queue(struct sr_core* sr);

/*-----------------------------------------------------------------------------
 * Component: arp_input(..) 
 *---------------------------------------------------------------------------*/


void sr_arp_input(struct sr_core* ,  /* -- borrowed -- */
                  uint8_t* packet,       /* -- given -- */
                  int len,
                  const struct sr_if* ); /* -- borrowed -- */

/*-----------------------------------------------------------------------------
 * Component: arp_output(..) 
 *---------------------------------------------------------------------------*/

int sr_arp_send_reply(struct sr_core* , 
                      struct sr_arphdr*     /* lent */, 
                      const struct sr_if* );

int sr_arp_send_request(struct sr_core* ,
                        uint32_t ip, /* nbo */
                        char* );

void sr_arp_output(struct sr_core* , 
                   uint8_t*          /* given */, 
                   uint32_t next_hop /*nbo*/,
                   char* interface);


#endif  /* -- SR_ARP_H -- */
