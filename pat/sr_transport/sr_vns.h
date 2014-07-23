/*-----------------------------------------------------------------------------
 * file:   sr_vns.h
 * date:   Fri Nov 21 13:06:27 PST 2003 
 * Author: Martin Casado 
 *
 * Description:
 *
 * Header file for interface to VNS.  Able to connect, reserve host,
 * receive/parse hardware information, receive/send packets from/to VNS.
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_VNS_H
#define SR_VNS_H

#ifdef _LINUX_
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>

#define SR_IFACE_NAMELEN 32

/* ----------------------------------------------------------------------------
 * struct sr_vns_if
 *
 * Abstraction for a VNS virtual host interface 
 *
 * -------------------------------------------------------------------------- */

struct sr_vns_if
{
    char name[SR_IFACE_NAMELEN];
    unsigned char addr[6];
    uint32_t ip;
    uint32_t subnet;
    uint32_t mask;
    uint32_t speed;
};

/* ----------------------------------------------------------------------------
 * struct sr_instance
 *
 * Encapsulation of the state for a single virtual router. 
 *
 * -------------------------------------------------------------------------- */

struct sr_instance
{
    /* VNS specific */
    int  sockfd;    /* socket to server */
    char user[32];  /* user name */
    char vhost[32]; /* host name */ 
    char lhost[32]; /* host name of machine running client */
    unsigned short topo_id; /* topology id */
    struct sockaddr_in sr_addr; /* address to server */
    FILE* logfile; /* file to log all received/sent packets to */
    volatile uint8_t  hw_init; /* bool : hardware has been initialized */

    void* interface_subsystem; /* subsystem to send/recv packets from */
};

/* ----------------------------------------------------------------------------
 * sr_instance "member" methods 
 * ---------------------------------------------------------------------------*/

void sr_destroy_instance(struct sr_instance* sr);
void sr_init_instance(struct sr_instance* sr, void*);

int sr_read_from_server(struct sr_instance* );

int sr_vns_send_packet(struct sr_instance* , uint8_t* , unsigned int , const char*);
int sr_connect_to_server(struct sr_instance* ,unsigned short , char* );
int sr_connected_to_server(struct sr_instance* );

void sr_set_user(struct sr_instance* sr);
void sr_init_log(struct sr_instance* sr, char* logfile);

void* sr_get_subsystem(struct sr_instance*);

/* ----------------------------------------------------------------------------
 * Integration methods for calling subsystem (e.g. the router).  These
 * may be replaced by callback functions that get registered with 
 * sr_instance.
 * -------------------------------------------------------------------------*/

void sr_vns_integ_init(struct sr_instance* );
void sr_vns_integ_hw_setup(struct sr_instance* ); /* called after hwinfo */
void sr_vns_integ_destroy(struct sr_instance* );
void sr_vns_integ_close(struct sr_instance* sr);
void sr_vns_integ_input(struct sr_instance* sr, 
                        const uint8_t * packet/* borrowed */,
                        unsigned int len,
                        const char* interface/* borrowed */);
void sr_vns_integ_add_interface(struct sr_instance*,
                                struct sr_vns_if* /* borrowed */);

int sr_vns_integ_output(struct sr_instance* sr /* borrowed */, 
                        uint8_t* buf /* borrowed */ ,
                        unsigned int len, 
                        const char* iface /* borrowed */);

#endif  /* SR_VNS_H */
