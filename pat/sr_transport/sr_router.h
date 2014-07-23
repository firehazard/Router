/*-----------------------------------------------------------------------------
 * File: sr_router.h 
 * Date: ?
 * Authors: Guido Apenzeller, Martin Casado, Virkam V.
 * Contact: casado@stanford.edu
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_ROUTER_H 
#define SR_ROUTER_H

#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>

#include "sr_arp.h"
#include "sr_protocol.h"
 
/* we dont like this debug , but what to do for varargs ? */
#ifdef _DEBUG_
#define Debug(x, args...) printf(x, ## args)
#define DebugIP(x) \
  do { struct in_addr addr; addr.s_addr = x; printf("%s",inet_ntoa(addr));\
     } while(0)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) printf("%02x:", \
  (unsigned char)(x[ivyl])); printf("%02x",(unsigned char)(x[5])); } while (0) 
#else
#define Debug(x, args...) do{}while(0)
#define DebugIP(x) do{}while(0)
#define DebugMAC(x) do{}while(0)
#endif

#define INIT_TTL 255 
#define PACKET_DUMP_SIZE 100 

/* forward declare */
struct sr_if;
struct sr_rt;

#define DROP_ARP_BADLEN   0 /* length of arp packet was bogus */
#define DROP_ARP_NOT_US   1 /* arp for someone else */
#define DROP_ARP_UNKNOWN  2 /* unknown ARP type */
#define DROP_IP_CSUM      3 /* bad ip header checksum */
#define DROP_IP_VERSION   4 /* ip version != 4 */
#define DROP_IP_BADHDL    5 /* ip header length isn't 5 32 bit words */
#define DROP_IP_BADLEN    6 /* ip packet length less than ethernet hdr + ip hdr
                             */
#define DROP_IP_BADTOTLEN 7 /* reported total length is inconsistent with
                               packet length */
#define DROP_ARP_BCAST    8 /* received arp claiming to be from the link layer
                               */
#define DROP_ETH_UNKNOWN  9 /* unknown ethernet type */

/* TEMP */
struct sr_instance;

/* ----------------------------------------------------------------------------
 * struct sr_stats
 *
 * -------------------------------------------------------------------------- */

struct sr_stats
{
    volatile uint32_t dropped[256];
};

/* ----------------------------------------------------------------------------
 * struct sr_config
 *
 * Configuration parameters for sr_instance.  Each of these should have
 * a default value set in sr_main.c::sr_init_instance(..)
 *
 * -------------------------------------------------------------------------- */

struct sr_config
{
};

struct sr_core
{
    /* Router State */
    struct sr_if* if_list; /* list of interfaces */
    struct sr_rt* routing_table; /* routing table */
    struct sr_config config; /* configurable parameters */
    struct sr_stats  stats;  /* bag o' statistics */
    struct arp_state arp_subsystem;

    void* network; /* pointer to "physical" network */
};


void sr_core_init(struct sr_core* cr, void* );
int sr_low_level_output(struct sr_core* core /* borrowed */, 
                        uint8_t* buf /* borrowed */ ,
                        unsigned int len, 
                        const char* iface /* borrowed */);

#endif /* SR_ROUTER_H */
