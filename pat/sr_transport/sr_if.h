/*-----------------------------------------------------------------------------
 * file:  sr_if.h
 * date:  Sun Oct 06 14:13:13 PDT 2002 
 * Contact: casado@stanford.edu 
 *
 * Description:
 *
 * Data structures and methods for handeling interfaces
 *
 * Note: This code is NOT thread safe! That means, no modifications
 *       by multiple-threads.  Multiple threads my access it read
 *       only however.
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_INTERFACE_H
#define SR_INTERFACE_H

#ifdef _LINUX_
#include <stdint.h>
#endif /* _LINUX_ */

#ifdef _SOLARIS_
#include </usr/include/sys/int_types.h>
#endif /* SOLARIS */

#ifdef _DARWIN_
#include <inttypes.h>
#endif

#define SR_IFACE_NAMELEN 32

struct sr_core;

/* ----------------------------------------------------------------------------
 * struct sr_if
 *
 * Node in the interface list for each router
 *
 * -------------------------------------------------------------------------- */

struct sr_if
{
    char name[SR_IFACE_NAMELEN];
    unsigned char addr[6];
    volatile uint32_t ip;
    uint32_t speed;
    volatile uint8_t  enabled;
    volatile uint32_t packets_in; 
    volatile uint32_t packets_out; 
    struct sr_if* next;
};

struct sr_if* sr_get_interface(struct sr_core* sr, const char* name);
void sr_add_interface(struct sr_core*, const char*);
void sr_set_ether_addr(struct sr_core*, const unsigned char*);
void sr_set_ether_ip(struct sr_core*, uint32_t ip_nbo);
void sr_print_if_list(struct sr_core*);
void sr_print_if(struct sr_if*);

int 
sr_ether_addrs_match_interface( struct sr_core* sr, /* borrowed */
                                uint8_t* buf, /* borrowed */
                                const char* name /* borrowed */ );

#endif /* --  SR_INTERFACE_H -- */
