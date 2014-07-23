/*
 * transport.c 
 *
 * CS244a HW#3C (Unreliable Transport)
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers.
 *
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctime>
#include <cstdlib> 
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"
#include <netinet/in.h>
#include <math.h>

enum { 
		CSTATE_CLOSED,
		CSTATE_LISTEN,
		CSTATE_SYN_SENT,
		CSTATE_SYN_RCVD,
		CSTATE_ESTABLISHED,
		CSTATE_FIN_WAIT1,
		CSTATE_FIN_WAIT2,
		CSTATE_TIME_WAIT,
		CSTATE_CLOSE_WAIT,
		CSTATE_LAST_ACK
		 };    

struct p_entry
{
	char* packet;
	struct timeval tstart; /* Time that packet was sent */
	int num_transmits; /* Number of time we sent this packet*/
	size_t packet_size; /* Total packet size including header */
	struct p_entry* next;
};


struct window
{
  struct p_entry* start;
  struct p_entry* end;
  unsigned int num_entries;
};

/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;
    
    /* Receiving/Sending buffers */
    window rbuffer;
    window sbuffer;
    
    /* Retransmissions */
    double rto;	 			/* current RTO value in miliseconds (up to microsec granularity)  */
	double estimated_rtt;   /* estimated RTT for calculating RTO */
	double devrtt; 			/* Deviation of RTT for calculation RTO*/
	  
    /* Window control */
    /* Receiver window (controls my sending) */
    tcp_seq  seq_base;    	 /*smallest seq number of transmitted but unacked packet (window back) */
    tcp_seq  nxt_seq_num; 	 /*next sequence number to be used (window front)*/
    uint16_t recv_win;		 /*Sampled receiver window - what remote guy is willing to accept */
        
    /*Senders window (controls my receiving) */
    tcp_seq last_ack_sent;   /*last ack sent - next in-order expected seq from other side*/		
    tcp_seq fin_ack;         /*expected incoming ack for sent fin*/

} context_t;

/****************** Function Prototypes ********************/
static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);
static bool active_init(mysocket_t sd, context_t *ctx);
static void init_window(struct window* win);
static bool passive_init(mysocket_t sd, context_t *ctx);
static void send_fin(mysocket_t sd, context_t *ctx);
static void handle_network_data(mysocket_t sd, context_t *ctx);
static void handle_app_data(mysocket_t sd, context_t *ctx);
static void handle_ack(struct tcphdr* hdr,  mysocket_t sd, context_t* ctx);
static void handle_fin(mysocket_t sd, context_t *ctx);
static bool sendrcv_syn(mysocket_t sd, context_t *ctx, struct tcphdr* in_packet);
static void add_to_send_buffer(context_t* ctx, char* pkt, size_t len);
static void add_to_recv_buffer(context_t *ctx, char* pkt, size_t len);
ssize_t network_recv(mysocket_t sd, void *dst);
ssize_t network_send(mysocket_t sd, const void *src, size_t src_len);
static void handle_recv_buffer(mysocket_t sd, context_t *ctx);
static void remove_until_seq(context_t *ctx);
static context_t* init_ctx(bool_t is_active);
static unsigned int wait_with_rto(mysocket_t sd, 
		unsigned int wait_flags, context_t *ctx);
static void send_ack(mysocket_t sd, context_t* ctx);
static void handle_timeout(mysocket_t sd, context_t *ctx);
static void free_buffer(struct window* win);
static void teardown_resources(context_t *ctx);
unsigned int control_wait(mysocket_t sd, context_t *ctx, bool fin_retry);
static unsigned int wait_for_event(mysocket_t sd, 
					unsigned int wait_flags, context_t *ctx, struct timeval start_time);
static void estimate_rto(context_t* ctx, tcp_seq incoming_ack);
static bool send_syn_ack(mysocket_t sd, context_t *ctx, struct tcphdr* syn_packet);
static bool is_valid_entry(context_t* ctx, tcp_seq target_seq, size_t data_len);
void our_dprintf(const char *format,...);

/************** CONSTANTS ********************************/
#define DEBUG 0
#define TH_MIN_OFFSET 5 /*tcp_hdr + padding */
#define TH_MAX_OFFSET 16 /*44 bytes of option + tcphdr + padding */
#define RWIN  3072
#define CWIN  3072
#define INIT_RTO 100.0
#define MAX_RT_TRIES 6 /*max number of retransmissions before having to kill someone */
#define MAX_RTO 500.0
#define ALPHA 0.125 /* Weight on sample_rtt vs. previous rtt */
#define BETA 0.25   /* Weight on new deviation vs. old deviation */ 
#define TIME_WAIT_VALUE 1000

/* ***************************************************
 * Function: transport_init
 * ***************************************************
 * initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
    context_t *ctx = init_ctx(is_active);
	bool success = false;
    if (is_active) {
		success = active_init(sd, ctx);		
    }
    else {
    	success = passive_init(sd, ctx);    	
    }
	if (success){  	
  	    ctx->connection_state = CSTATE_ESTABLISHED;
	    stcp_unblock_application(sd);
	    control_loop(sd, ctx);
	}
	else {
		errno = ETIMEDOUT;
		stcp_unblock_application(sd);
	}
	teardown_resources(ctx);
	/* Wait for potential pending processes in 
	 * other side (this is to fix a bug in the starter code) */
	sleep(1);		
	our_dprintf("Exiting Loop!\n");
}

/* ***************************************************
 * Function: teardown_resources
 * ***************************************************
 * 	Free allocated memory such as sender/receiver window
 */
static void teardown_resources(context_t *ctx)
{
	free_buffer(&(ctx->sbuffer));
	free_buffer(&(ctx->rbuffer));
	free(ctx);
}

/* ***************************************************
 * Function: free_buffer
 * ***************************************************
 * 	Iterate through buffer freeing allocated memory
 */
static void free_buffer(struct window* win)
{
	struct p_entry* curr = win->start->next;	
	struct p_entry* victim;
	while (curr) {
		victim = curr;
		curr = curr->next;
		free(victim->packet);
		free(victim);
	}
	free(win->start);
}

/* ***************************************************
 * Function: init_ctx
 * ***************************************************
 * 	Initiate the connection context structure
 */
static context_t* init_ctx(bool_t is_active)
{
	srand((unsigned)time(0));
	context_t *ctx = (context_t *) calloc(1, sizeof(context_t));
	assert(ctx);
	ctx->connection_state = CSTATE_CLOSED;
	generate_initial_seq_num(ctx);
	if (is_active){
		/* Because time(0) can be the same for client/server due to low
		 * granularity, call it again for one side */
		generate_initial_seq_num(ctx); 
	}
	init_window(&(ctx->sbuffer));
	init_window(&(ctx->rbuffer));
	ctx->estimated_rtt = 0;
	ctx->devrtt = 0;
	ctx->rto = INIT_RTO;
	return ctx;
}

/* ***************************************************
 * Function: init_window
 * ***************************************************
 * 	Helper function for initializing a buffer link list
 * chain
 */
static void init_window(struct window* win)
{
	win->start = (struct p_entry*)calloc(1,sizeof(struct p_entry));
	win->end = win->start;
}

/* ***************************************************
 * Function: wait_with_rto
 * ***************************************************
 * 	Wrapper function for wait_for_event that uses the time
 *  that the packet in the head of the window was sent
 *  as the starting time.
 */
static unsigned int wait_with_rto(mysocket_t sd, 
					unsigned int wait_flags, context_t *ctx)
{
	assert(ctx->sbuffer.start->next || !DEBUG);
	struct timeval start_time = ctx->sbuffer.start->next->tstart;
	unsigned int event = wait_for_event(sd, wait_flags, ctx, start_time); 
	return event;
}

/* ***************************************************
 * Function: wait_for_event
 * ***************************************************
 * 	This function does the dirty work of adding the starting time to the
 *  current RTO to determine the finish time at which stcp_wait_for_event
 *  should TIMEOUT.  
 */
static unsigned int wait_for_event(mysocket_t sd, 
					unsigned int wait_flags, context_t *ctx, struct timeval start_time)
{
	unsigned int total_microsecs = (int)(ctx->rto*1000.0) + start_time.tv_usec; 
	unsigned int extra_seconds = (int)(total_microsecs/1000000);
	struct timespec finish_time;
	finish_time.tv_sec = start_time.tv_sec + extra_seconds;
	unsigned int microseconds = total_microsecs - (extra_seconds*1000000);
	finish_time.tv_nsec = (microseconds*1000);
	struct timeval curr_time;
	gettimeofday(&curr_time, NULL);
/*	our_dprintf("Curr sec: %d  ,micro: %d \n", curr_time.tv_sec, curr_time.tv_usec);
	our_dprintf("Start sec: %d ,micro: %d \n", start_time.tv_sec, start_time.tv_usec);
	our_dprintf("End   sec: %d ,micro: %d \n", finish_time.tv_sec, finish_time.tv_nsec/1000);*/
	unsigned int event = stcp_wait_for_event(sd,wait_flags,&finish_time);
	return event; 
						
						
}

/* ***************************************************
 * Function: active_init
 * ***************************************************
 * 	Initiate the active side connection.  Send the SYN packet,
 * wait for the SYN_ACK and then send the final SYN. 
 */
static bool active_init(mysocket_t sd, context_t *ctx)
{
	
	struct tcphdr* syn_packet = (struct tcphdr *)calloc(1,(TH_MAX_OFFSET*sizeof(uint32_t)) + STCP_MSS);	
	if (!sendrcv_syn(sd, ctx, syn_packet)) {
		return false;	/* Timeout waiting for SYN_ACK*/
	}
	
	ctx->connection_state = CSTATE_SYN_SENT;
	our_dprintf("Active - RVCD SYN_ACK Seq: %d, ACK: %d \n", syn_packet->th_seq, syn_packet->th_ack);
	ctx->recv_win = MIN(syn_packet->th_win, CWIN);

	/* Send final ACK ---- Prepare Headers */
	syn_packet->th_ack = syn_packet->th_seq + 1; 
	syn_packet->th_seq = ctx->nxt_seq_num;		 
	syn_packet->th_flags = TH_ACK;
	syn_packet->th_off = TH_MIN_OFFSET;
	syn_packet->th_win = RWIN;
	our_dprintf("Active - Sending Final ACK Seq: %d, ACK: %d \n", syn_packet->th_seq, syn_packet->th_ack);
	
	network_send(sd, syn_packet, (TH_MIN_OFFSET*sizeof(uint32_t)));
	
	/* Update connection state variables */
	ctx->seq_base = ctx->nxt_seq_num;
	ctx->last_ack_sent = syn_packet->th_ack;
	free(syn_packet);
	return true;
}

/* ***************************************************
 * Function: sendrcv_syn
 * ***************************************************
 * 	Helper function for active-side network initiation.
 *   It sends the SYN packet and waits for the right SYN_ACK
 */
static bool sendrcv_syn(mysocket_t sd, context_t *ctx, struct tcphdr* in_packet)
{
	/* Set SYN Headers */
	struct tcphdr* out_packet = (struct tcphdr *)calloc(1,(TH_MIN_OFFSET*sizeof(uint32_t)));
	out_packet->th_ack = 0; 
	out_packet->th_seq = ctx->initial_sequence_num;
  	out_packet->th_flags = TH_SYN;
	out_packet->th_off = TH_MIN_OFFSET;
	out_packet->th_win = RWIN;
	ctx->nxt_seq_num = ctx->initial_sequence_num + 1; /* SYN Packet =  1 byte */
	our_dprintf("Active Sending SYN.  Seq:%d\n", out_packet->th_seq);
	
	unsigned int event;
	bool success = false;
	int num_tries = 1;
	struct timeval tstart;
	gettimeofday(&tstart,NULL); /*start the timer */
	network_send(sd, out_packet, (TH_MIN_OFFSET*sizeof(uint32_t)));
	/* Try sending the SYN a total of 6 times before giving up */
	while ((num_tries < MAX_RT_TRIES) && !success){
		event = wait_for_event(sd, NETWORK_DATA, ctx, tstart);
		if (event & NETWORK_DATA) {
			network_recv(sd, in_packet);
			/* Check to see if we got the right SYN_ACK */
			success = ((in_packet->th_flags & (TH_SYN | TH_ACK)) &&
			    	  (in_packet->th_ack == ctx->nxt_seq_num));
		}
		else if (event==TIMEOUT){
			our_dprintf("Active - SYN TIMEOUT!, no SYN_ACK!\n");
			gettimeofday(&tstart,NULL);
			num_tries+=1;
			our_dprintf("Active Resending SYN.  Seq:%d\n", out_packet->th_seq);
			network_send(sd, out_packet, (TH_MIN_OFFSET*sizeof(uint32_t)));
		}
	}	
	free(out_packet);
	return success;
}

/* ***************************************************
 * Function: network_recv
 * ***************************************************
 * Wrapper function for receiving network data.  It converts
 * data in the headers into host byte order.
 */
ssize_t network_recv(mysocket_t sd, void *dst)
{
	ssize_t result;
	/* Receive up to 44 bytes of TCP options */
	while ((result = stcp_network_recv(sd, dst, 
		   (TH_MAX_OFFSET*sizeof(uint32_t)) + STCP_MSS))<0);
	struct tcphdr* hdr = (struct tcphdr *)dst;
	hdr->th_ack = ntohl(hdr->th_ack);
	hdr->th_seq = ntohl(hdr->th_seq);
	hdr->th_win = ntohs(hdr->th_win);
	return result;	
}

/* ***************************************************
 * Function: network_send
 * ***************************************************
 * Wrapper function for sending network data.  It converts header fields
 * into network byte-order but then changes them back before returning
 * in case the caller plans to use that packet later on.
 */
ssize_t network_send(mysocket_t sd, const void *src, size_t src_len)
{
	struct tcphdr* hdr = (struct tcphdr *)src;
	hdr->th_ack = htonl(hdr->th_ack);
	hdr->th_seq = htonl(hdr->th_seq);
	hdr->th_win = htons(hdr->th_win);
	hdr->th_flags = hdr->th_flags;
	hdr->th_off = hdr->th_off;	
	ssize_t result;
	while ((result = stcp_network_send(sd, src, src_len, NULL))<0);
	hdr->th_ack = ntohl(hdr->th_ack);
	hdr->th_seq = ntohl(hdr->th_seq);
	hdr->th_win = ntohs(hdr->th_win);
	return result;	
}

/* ***************************************************
 * Function: passive_init
 * ***************************************************
 * Passive node waits for a SYN packet to arrive, it then
 * sends an SYN_ACK using another helper function (below)
 */
static bool passive_init(mysocket_t sd, context_t *ctx)
{
	ctx->connection_state = CSTATE_LISTEN;
	struct tcphdr* syn_packet = (struct tcphdr *)calloc(1,
				(TH_MAX_OFFSET*sizeof(uint32_t))+STCP_MSS);
   /* Listen for a connection request */
	while (true){
		stcp_wait_for_event(sd, NETWORK_DATA, NULL);
 		network_recv(sd, syn_packet);
		if (syn_packet->th_flags == TH_SYN){
			break;	
		}
	}
	our_dprintf("PASSIVE - RCVD SYN with seq: %d\n", syn_packet->th_seq);
	
	/* Prepare SYN_ACK*/			
	syn_packet->th_ack = syn_packet->th_seq + 1;
	syn_packet->th_seq = ctx->initial_sequence_num;
	syn_packet->th_off = TH_MIN_OFFSET;
	syn_packet->th_flags = TH_SYN | TH_ACK;
	syn_packet->th_win = RWIN;
	/*Update connection state variables */	
	ctx->last_ack_sent = syn_packet->th_ack;
	ctx->nxt_seq_num = ctx->initial_sequence_num + 1;
	ctx->connection_state = CSTATE_SYN_RCVD;
	ctx->seq_base = ctx->nxt_seq_num; 
	our_dprintf("PASSIVE - SEND SYN_ACK Packet with seq: %d and ack: %d \n",
				 syn_packet->th_seq, syn_packet->th_ack);
	bool success = send_syn_ack(sd, ctx, syn_packet);

	free(syn_packet);
	return success;
}

/* ***************************************************
 * Function: send_syn_ack
 * ***************************************************
 * Passive node calls this to send SYN_ACK and wait for
 * the final ACK packet. It tries a total of 6 times before
 * giving up.
 */
static bool send_syn_ack(mysocket_t sd, context_t *ctx, struct tcphdr* syn_packet)
{
	struct tcphdr* ack_packet = (struct tcphdr *)calloc(1,(TH_MAX_OFFSET*sizeof(uint32_t))+STCP_MSS);
	unsigned int event;
	bool success = false;
	int num_tries = 1;
	struct timeval tstart;
	gettimeofday(&tstart, NULL); /*Start the timer */
	network_send(sd, syn_packet, (TH_MIN_OFFSET*sizeof(uint32_t)));
	while((num_tries < MAX_RT_TRIES) && !success){
		event = wait_for_event(sd, NETWORK_DATA, ctx, tstart);
		if (event & NETWORK_DATA) {
			network_recv(sd, ack_packet);
			our_dprintf("Passive (ACK waiting): Ack: %d, Seq:%d, Next Seq:%d, Last Ack Sent: %d\n", 
						 ack_packet->th_ack, ack_packet->th_seq, ctx->nxt_seq_num, ctx->last_ack_sent);
			success =  ((ack_packet->th_flags == TH_ACK) &&
						(ack_packet->th_ack == ctx->nxt_seq_num));
		}
		else if (event==TIMEOUT){
			our_dprintf("Passive Final INIT ACK not Received TIMEOUT!\n");	
			num_tries+=1;
			gettimeofday(&tstart, NULL);
			our_dprintf("PASSIVE - SEND SYN_ACK Packet with seq: %d and ack: %d \n",
						 syn_packet->th_seq, syn_packet->th_ack);
			network_send(sd, syn_packet, (TH_MIN_OFFSET*sizeof(uint32_t)));
		}
	}
	if (success){
		our_dprintf("PASSIVE - RCVD Last Ack Packet has seq: %d and ack: %d \n",
					 ack_packet->th_seq, ack_packet->th_ack);
   		ctx->recv_win = MIN(syn_packet->th_win, CWIN);
	}	
	free(ack_packet);
	return success;
}






/* generate random initial sequence number for an STCP connection */
static void generate_initial_seq_num(context_t *ctx)
{
    assert(ctx);

#ifdef FIXED_INITNUM
    /* please don't change this! */
    ctx->initial_sequence_num = 1;
#else
    ctx->initial_sequence_num = (int)(rand()/(((double)RAND_MAX + 1)/ 256));
#endif
}


/* ***************************************************
 * Function: control_loop
 * ***************************************************
 * control_loop() is the main STCP loop; it repeatedly waits for one of the
 * following to happen:
 *   - incoming data from the peer
 *   - new data from the application (via mywrite())
 *   - the socket to be closed (via myclose())
 *   - a timeout
 */
static void control_loop(mysocket_t sd, context_t *ctx)
{
    assert(ctx);
	unsigned int event = 0;
	bool fin_retry = false; /* TRUE = APP_CLOSE_REQUESTED but window was full */
    while (!ctx->done)
    {
    	size_t bytes_in_transit = ctx->nxt_seq_num - ctx->seq_base;
  		size_t win_space = ctx->recv_win - bytes_in_transit;
    	event = control_wait(sd, ctx, fin_retry);
		if (event & NETWORK_DATA) {
		   handle_network_data(sd, ctx);
		}
		if ((event & APP_DATA) && !fin_retry) {
			handle_app_data(sd, ctx); 
		}
		if (event == TIMEOUT){
			handle_timeout(sd, ctx);

		}
		if ((event & APP_CLOSE_REQUESTED) || fin_retry) {
   			if (win_space>0){
				send_fin(sd, ctx);
				fin_retry = false;
   			}
   			else {
   				our_dprintf("App requested FIN but window is full\n");
   				fin_retry = true;	
   			}
		 }
    }
}


/* ***************************************************
 * Function: control_wait
 * ***************************************************
 * This function determines the right event to wait for based on window size
 * and connection state.
 * If the window is full, then we don't wait for application data as we know
 * we can't send any.
 * If there are bytes outstanding (not-acked), then we need to wait using a timeout
 * value.
 * If we are TIME_WAIT state, then we set the RTO to the TIME_WAIT_VALUE and only accept
 * network data (i.e. FIN that need to be acknowledged).
 */
unsigned int control_wait(mysocket_t sd, context_t *ctx, bool fin_retry)
{
	unsigned int event = 0;
	size_t bytes_in_transit = ctx->nxt_seq_num - ctx->seq_base;
  	size_t win_space = ctx->recv_win - bytes_in_transit;
  		
	unsigned int wait_flags = NETWORK_DATA | APP_CLOSE_REQUESTED;
	if (win_space >0 && !fin_retry) {
		wait_flags |= APP_DATA;	
	}
	/* Is there something in flight? */
	if (bytes_in_transit > 0) {
		assert((ctx->sbuffer.num_entries>0) || !DEBUG);
		our_dprintf("Waiting with RTO for Seq:%d \n", ctx->seq_base);
		event = wait_with_rto(sd, wait_flags, ctx); 
	}
	else if (ctx->connection_state==CSTATE_TIME_WAIT) {
		ctx->rto = TIME_WAIT_VALUE;
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		our_dprintf("IN TIME WAIT!!!\n");
		event = wait_for_event(sd, NETWORK_DATA, ctx,current_time);    			
	}
	else {
		/* No unacked packets... take your sweet time*/
		event = stcp_wait_for_event(sd, ANY_EVENT, NULL);
	}
	return event;	
}

/* ***************************************************
 * Function: handle_timeout
 * ***************************************************
 * This function is called whenever there is a timeout. Unless we
 * are in the TIME_WAIT state, I retransmit all the packets starting
 * from the beginning of the window.  
 * The RTO is also doubled when there is a retransmission as described
 * in K&R. 
 */
static void handle_timeout(mysocket_t sd, context_t *ctx)
{	
	if (ctx->connection_state==CSTATE_TIME_WAIT){
		ctx->connection_state = CSTATE_CLOSED;
		ctx->done = true;
		return;
	}
	our_dprintf("TIMEOUT waiting for seq:%d\n", ctx->seq_base);	
	assert(ctx->sbuffer.num_entries>0 || !DEBUG);
	assert(ctx->sbuffer.start->next!=NULL || !DEBUG);
	struct p_entry *curr = ctx->sbuffer.start->next;
	ctx->rto= MIN(ctx->rto*2, MAX_RTO);
	/* Retransmit all packets */
	while (curr){
			struct tcphdr *hdr = (struct tcphdr*)curr->packet;
			if (curr->num_transmits>=MAX_RT_TRIES){
				our_dprintf("MAX NUMBER OF TRIES REACHED KILLING CONNECTION !!!!\n\n");
				ctx->done = true;
				errno = ETIMEDOUT;
				return;
			}
			hdr->th_ack = ctx->last_ack_sent;
			curr->num_transmits+=1; /* increase counter */
			gettimeofday(&(curr->tstart),NULL);
			network_send(sd, hdr, curr->packet_size);
			curr = curr->next;
			our_dprintf("Retransmitting Seq:%d with Len:%d New RTO:%f\n",
				 hdr->th_seq, ctx->sbuffer.start->next->packet_size, ctx->rto);
	}
}


/* ***************************************************
 * Function: send_fin
 * ***************************************************
 * The application has requested to be closed and there
 * is at least 1 byte of room in the window. This function
 * prepares a fin packet, adds it to the send buffer, and
 * sends it.  It also updates the connection state.
 */
static void send_fin(mysocket_t sd, context_t *ctx)
{
	our_dprintf("App requested close there is space in window... SENDING FIN\n");
  	struct tcphdr* fin_pkt = (struct tcphdr *)calloc(1,TH_MIN_OFFSET*sizeof(uint32_t));
  	fin_pkt->th_seq = ctx->nxt_seq_num;
  	ctx->fin_ack = fin_pkt->th_seq + 1;
  	ctx->nxt_seq_num += 1; /*FIN = 1 byte of data */
  	fin_pkt->th_off = TH_MIN_OFFSET;
  	fin_pkt->th_flags = TH_FIN;
  	fin_pkt->th_win = RWIN;
  	/* Update State variables depending on previous state */
  	if (ctx->connection_state == CSTATE_ESTABLISHED ) {
  		ctx->connection_state = CSTATE_FIN_WAIT1;
  	}
  	else {
  		assert(ctx->connection_state == CSTATE_CLOSE_WAIT || !DEBUG);
  		ctx->connection_state = CSTATE_LAST_ACK;
  	}
  	add_to_send_buffer(ctx, (char *)fin_pkt, (TH_MIN_OFFSET*sizeof(uint32_t)));
  	network_send(sd, fin_pkt, TH_MIN_OFFSET*sizeof(uint32_t));
	free(fin_pkt);
}

/* ***************************************************
 * Function: handle_app_data
 * ***************************************************
 * The application has new data to be sent out.  First make sure that
 * there is some space in the window.  Get maximum amount of data from 
 * the application, added it to the sender's buffer, and send it across
 * the network.
 */
static void handle_app_data(mysocket_t sd, context_t *ctx)
{
   size_t bytes_in_transit = ctx->nxt_seq_num - ctx->seq_base;
   size_t win_space = ctx->recv_win - bytes_in_transit;
   if (win_space>0){
       char *pkt = (char *)calloc(1,(TH_MIN_OFFSET*sizeof(uint32_t)) + STCP_MSS);
       struct tcphdr* hdr = (struct tcphdr*)pkt;
       hdr->th_off = TH_MIN_OFFSET;
       hdr->th_seq = ctx->nxt_seq_num;
       hdr->th_flags = 0;
       hdr->th_win = RWIN;
       /* Pick the minimum between space in the window and MSS */
       size_t data_max = MIN(STCP_MSS, win_space);	           
       size_t len = stcp_app_recv(sd, pkt + TCP_DATA_START(hdr), data_max); 	           
       ctx->nxt_seq_num += len; /*add len to obtain nxt_seq_num to be used */
       add_to_send_buffer(ctx,  pkt, (TH_MIN_OFFSET*sizeof(uint32_t)) + len);
 	   our_dprintf("Sending packet with seq: %d of size: %d\n", 
 	   				hdr->th_seq,(TH_MIN_OFFSET*sizeof(uint32_t)) + len);
  	   network_send(sd, pkt, (TH_MIN_OFFSET*sizeof(uint32_t)) + len);
       free(pkt);
   }
   else {
   		our_dprintf(".");           	
   }	
}


/* ***************************************************
 * Function: add_to_send_buffer
 * ***************************************************
 *  A data/fin packet is being sent out. This function adds that packet
 *  to the sender's window (end of link list) in case it timeouts and 
 *  needs to be retransmitted. It records the time the packet was sent.
 */
static void add_to_send_buffer(context_t* ctx, char* pkt, size_t len)
{
	struct window* win = &(ctx->sbuffer);
	assert((win->end->next==NULL) || !DEBUG);
	struct p_entry* new_entry = (struct p_entry*)calloc(1,sizeof(struct p_entry));
	new_entry->packet = (char *)calloc(1, len);
	memcpy(new_entry->packet, pkt, len);
	new_entry->packet_size = len;
	new_entry->next = NULL;
	new_entry->num_transmits = 1;
	gettimeofday(&(new_entry->tstart), NULL);
	win->end->next = new_entry; /* Attach to the end of the list */
	win->end = win->end->next;  /* Update the end of the list */
	win->num_entries+=1;		/* Add 1 to the number of entries */
	assert(win->start->next || !DEBUG);
	our_dprintf("Added Seq: %d to send buffer\n", ((struct tcphdr*)pkt)->th_seq);
} 


/* ***************************************************
 * Function: handle_network_data
 * ***************************************************
 *  A packet has arrived.  If its an ACK packet, call a helper 
 * function that deals with acks.  If the packet has data or it's
 * a FIN packet, then add the packet to the receiver buffer only
 * if it fits in the receiving window.  If it doesn't fit,
 * drop the packet.  If it fits, send an acknowledgement.
 */
static void handle_network_data(mysocket_t sd, context_t *ctx)
{
	   char *pkt = (char *)calloc(1, (TH_MAX_OFFSET*sizeof(uint32_t)) + STCP_MSS);
	   size_t total_length = network_recv(sd, pkt);
	   struct tcphdr* hdr = (struct tcphdr *)pkt;
	   ctx->recv_win = MIN(hdr->th_win, CWIN);
	   if (hdr->th_flags & TH_ACK) {
			handle_ack(hdr, sd, ctx);
	   }
	   size_t data_length = total_length - TCP_DATA_START(hdr);
	   if (data_length>0 || (hdr->th_flags & TH_FIN)) {
	   		our_dprintf("Received %d bytes of data with seq: %d, last ack sent: %d\n", 
	   					data_length, hdr->th_seq, ctx->last_ack_sent);
	   		/* Does the packet fit in the receiving window?*/
	   		if ((hdr->th_seq + data_length)<=(ctx->last_ack_sent + RWIN)) {
	   			add_to_recv_buffer(ctx, pkt, total_length);
	   			if (hdr->th_seq == ctx->last_ack_sent){
	   				/* The packet arrived in order. We can remove all in-order
	   				 * packets from the receive buffer now */
	   				our_dprintf("Packet In Order. Handling buffer\n");
	   				handle_recv_buffer(sd, ctx);
	   			}
	   			else {
	   				if (hdr->th_seq > ctx->last_ack_sent) {
	   					our_dprintf("This packet is out of order. Adding to recv buffer\n");
	   				}
	   			}
	   			send_ack(sd, ctx);
	   		}
	   		else {
	   			our_dprintf("Packet outside receiver buffer. Dropping!!!\n");	
	   	
	   		}		
	   }
	   free(pkt);			
}


/* ***************************************************
 * Function: handle_recv_buffer
 * ***************************************************
 *  This function is called whenever we get the next packet
 *  we are expecting from the sender in-order.  It iterates
 *  through the receive buffer sending packets to the application
 * for consumption.  When a packet is consumed, it is freed and
 * removed from the buffer.  The loop stops as soon as we see
 * that the next packet is not in order or when we have emptied out
 * the buffer entirely (whichever happens first).  
 */
static void handle_recv_buffer(mysocket_t sd, context_t *ctx)
{
	struct window* win = &(ctx->rbuffer);
	struct p_entry* curr = win->start->next;
	assert(curr || !DEBUG);
	tcp_seq next_seq, curr_seq;
	size_t data_len;
	bool is_fin = false;
	bool stop_loop = false;
	/* Iterate through all entries in the receive buffer */
	while (win->num_entries>0 && !stop_loop) {
		data_len = curr->packet_size - TCP_DATA_START(curr->packet);
		curr_seq = ((struct tcphdr *)curr->packet)->th_seq;
		/* Special case, FIN packet in receive buffer */
		is_fin = (((struct tcphdr *)curr->packet)->th_flags & TH_FIN);
		if (is_fin){
			assert(curr->next==NULL || !DEBUG);
			ctx->last_ack_sent+=1;
			if (data_len>0) {
				/* FIN packet with data.  Send data to app */
				stcp_app_send(sd, curr->packet + TCP_DATA_START(curr->packet), data_len);
			}
			handle_fin(sd, ctx); /* Call helper function */
			break;	
		}
		our_dprintf("Sending packet with Seq: %d to application \n", curr_seq);
		ctx->last_ack_sent = curr_seq + data_len;
		stcp_app_send(sd, curr->packet + TCP_DATA_START(curr->packet), data_len);
		/* Have we reached the end of the buffer?*/
		if (curr->next){
			/* No, exit only if next sequence is not in order */
			next_seq = ((struct tcphdr *)curr->next->packet)->th_seq;	
			if ((data_len + curr_seq)!=next_seq){
				stop_loop = true;	
			} 	
		}
		/* Update pointers and free memory */
		win->start->next = curr->next;
		if (win->end == curr){
			win->end = win->start;	
		}
		free(curr->packet);
		free(curr);
		curr = win->start->next;
		win->num_entries-=1;	
	}
}

/* ***************************************************
 * Function: add_to_recv_buffer
 * ***************************************************
 *  This function simply adds a packet (if its a valid packet, defined
 *  by the next function) to the receiver buffer.  It searches the receiver
 *  buffer for the right place to put the file.  The receive buffer is always
 *  kept in anscending order of sequence numbers.   
 */
static void add_to_recv_buffer(context_t *ctx, char* pkt, size_t len)
{	
	tcp_seq target_seq = ((struct tcphdr *)pkt)->th_seq;
	size_t data_len = len - TCP_DATA_START(pkt);	
	if (!is_valid_entry(ctx,target_seq, data_len)){
		our_dprintf("Invalid incoming packet - dropping!\n");
		return;	
	}
	struct window* win = &(ctx->rbuffer);
	assert((win->end->next==NULL) || !DEBUG);
	/* Create a new link list entry */
	struct p_entry* new_entry = (struct p_entry*)calloc(1,sizeof(struct p_entry));
	new_entry->packet = (char *)calloc(1, len);
	memcpy(new_entry->packet, pkt, len);
	new_entry->packet_size = len;
	tcp_seq curr_seq;
	struct p_entry* prev = win->start;
	struct p_entry* curr = win->start->next;
	bool inserted = false;
	/* Search for the right place to add this sequence */
	while(curr) {
		curr_seq = ((struct tcphdr *)curr->packet)->th_seq;
		if (target_seq < curr_seq) {
			/* Place found */
			inserted = true;
			new_entry->next = curr;
			prev->next = new_entry;
			break;
		}
		prev = curr;
		curr = curr->next;
	}
	/* Add to the end of the list if not added before */
	if (!inserted){
		new_entry->next = NULL;	
		win->end->next = new_entry;
		win->end = win->end->next;
	}
	win->num_entries+=1;
	our_dprintf("Added Recv: %d to receive buffer\n", ((struct tcphdr*)pkt)->th_seq);
}

/* ***************************************************
 * Function: is_valid_entry
 * ***************************************************
 *  Determines if an incoming packet "fits" in the receive buffer. 
 *  The following conditions must be sastified.  
 * 	1.) The incoming sequence number better be greater than or equal to
 * 	    the last ack we sent out, otherwise we already have the data.
 *  2.) The incoming sequence number cannot be identical to any sequence
 * 		number in the receive buffer.  This just means we have a duplicate
 * 		packet so no need to store it.
 *  3.) If the packet fits between sequence X and sequence Y, its length
 * 		better not be large enough to overlap with Y, and the length of X
 * 		better not be large enough to overlap with the packet's sequence.
 * 		This means the packet is trying to overwrite data and I simply drop it.
 */
static bool is_valid_entry(context_t* ctx, tcp_seq target_seq, size_t data_len)
{
	if (target_seq< ctx->last_ack_sent){
		our_dprintf("Received Packet Outside receiver window with seq: %d\n", target_seq);
		return false;	
	}
	struct p_entry* prev = ctx->rbuffer.start;
	struct p_entry* curr = ctx->rbuffer.start->next;
	while(curr) {
		tcp_seq curr_seq = ((struct tcphdr *)curr->packet)->th_seq;
		if (target_seq==curr_seq){
			our_dprintf("Received Duplicate Packet with seq: %d\n", target_seq);
			return false;
		}
		else if (target_seq < curr_seq) {
			if (curr->next){
				tcp_seq next_seq = ((struct tcphdr *)curr->next->packet)->th_seq;	
				if ( (target_seq + data_len) > next_seq) {
					our_dprintf("Packet overwriting 1!!! TARGET SEQ:%d DATA_LEN:%d NEXT_SEQ:%d CURR SEQ:%d\n",
								 target_seq, data_len, curr_seq);
					assert(!DEBUG);
					return false;
				}	
			}
			if (prev!=ctx->rbuffer.start) {
				tcp_seq prev_seq = ((struct tcphdr *)prev->packet)->th_seq;
				size_t prev_len = prev->packet_size - TCP_DATA_START(prev->packet);
				if ((target_seq>prev_seq) && ((prev_seq + prev_len) > target_seq)){
					our_dprintf("Packet overwriting 2!!! PREV SEQ:%d PREV_LEN:%d TARGET_SEQ:%d CURR SEQ:%d\n",
								 prev_seq, prev_len, target_seq, curr_seq);
					assert(!DEBUG);
					return false;
				}		
				
			}
		}
		prev = curr;
		curr = curr->next;
	}
	return true;
}

/* ***************************************************
 * Function: handle_fin
 * ***************************************************
 * This function is called when we receive a FIN (or actually
 * when we process a FIN in-order from the receive buffer).  It
 * simply updates the states variables and calls stcp_fin_received.
 * 
 */
static void handle_fin(mysocket_t sd, context_t *ctx)
{
   	size_t bytes_in_transit = ctx->nxt_seq_num - ctx->seq_base;
   	our_dprintf("FIN PACKET RECEIVED. ACK SEN. Window Start:%d Window End:%d\n",
   				 ctx->seq_base, ctx->nxt_seq_num);
    
    /* This is a special case, we sent a FIN first but we received another 
     * FIN before we got an ACK for the first FIN.  If this is the case, I just
     * treat that second FIN as an ACK for the first FIN (though this would not be
     * necessarily true if both sides were to send FINs simulatenously). We were
     * not asked to deal with simulatenous shut-down so I just do it this clean
     * way */
    if (ctx->connection_state == CSTATE_FIN_WAIT1) {
    	assert(((ctx->seq_base + 1) == ctx->nxt_seq_num) || !DEBUG);
    	ctx->seq_base = ctx->nxt_seq_num;
    	remove_until_seq(ctx);
    	ctx->connection_state = CSTATE_TIME_WAIT;
    }	
    else if (ctx->connection_state == CSTATE_FIN_WAIT2) {
		assert(bytes_in_transit==0 || !DEBUG);
		ctx->connection_state = CSTATE_TIME_WAIT;
	}
	else  {
		assert(ctx->connection_state = CSTATE_ESTABLISHED || !DEBUG);
		assert(bytes_in_transit==0 || !DEBUG);
		ctx->connection_state = CSTATE_CLOSE_WAIT;
	}
	stcp_fin_received(sd);
}

/* ***************************************************
 * Function: handle_ack
 * ***************************************************
 * We received an ACK.  Check the special case that we received
 * a duplicate SYN_ACK and if so, send an ACK back (this is the only 
 * ACK packet that requires an ACK to be sent back). Then check if the
 * ACK falls within the sender window and if so, estimate the new rto
 * (using helper function below), remove everything upto that sequence
 * number from the window. Finally, check to see if this is a FIN ACK
 * and update state variables.
 */
static void handle_ack(struct tcphdr* hdr,  mysocket_t sd, context_t* ctx)
{
	our_dprintf("Ack:%d received - Prior Window Bottom:%d - Window Top: %d\n",
				 hdr->th_ack, ctx->seq_base, ctx->nxt_seq_num);	
	if (hdr->th_flags & TH_SYN) {
		/* Received a duplicate SYN_ACK during init - can only happen first time around for client */
		assert(((hdr->th_seq == ctx->last_ack_sent-1) && 
			(hdr->th_ack==ctx->initial_sequence_num+1)) || !DEBUG);
		if((hdr->th_seq == ctx->last_ack_sent-1) && 
			(hdr->th_ack==ctx->initial_sequence_num+1)){
			send_ack(sd,ctx); 
			return;
		}
	}
	if (!((hdr->th_ack <= ctx->nxt_seq_num) && (hdr->th_ack > ctx->seq_base))) {
		our_dprintf("Ack outside window. Throw Away!\n");
		return;	
	}
	/* ACK INSIDE WINDOW - HANDLE IT */
	estimate_rto(ctx, hdr->th_ack); /* check if we can get a new RTO*/
	ctx->seq_base = hdr->th_ack; /* new window bottom */
	remove_until_seq(ctx); /* remove everything upto the new window bottom (cumulative acks)*/
	
	/* Check to see if we got ACK for FIN.  If so update state variables*/
	if (hdr->th_ack == ctx->fin_ack) {
   		if (ctx->connection_state == CSTATE_FIN_WAIT1) {
   			ctx->connection_state = CSTATE_FIN_WAIT2;				
   		}
   		else if (ctx->connection_state == CSTATE_LAST_ACK) {
   			assert((ctx->nxt_seq_num==ctx->seq_base) || !DEBUG);
   			ctx->connection_state = CSTATE_CLOSED;
   			ctx->done = 1;   
   		}
	}
}

/* ***************************************************
 * Function: estimate_rto
 * ***************************************************
 * When we receive an ACK, we iterate through the send window in 
 * order to find the packet corresponding to such ACK.  When we find
 * the packet, we first make sure that this is not a retransmitted packet
 * and if so we update the estimated rtt and rtt deviation based on the
 * equations from K&R. Note that I use 3*estimate_rtt in my equation 
 * simply because a factor of 1 was too low and caused 5 retransmissions
 * way too quickly.   
 */
static void estimate_rto(context_t* ctx, tcp_seq incoming_ack)
{
	struct p_entry* curr = ctx->sbuffer.start->next;
	size_t data_len;
	tcp_seq curr_seq; 
	struct timeval curr_time;
	assert(curr || !DEBUG);
	while (curr) {
		curr_seq = ((struct tcphdr*)curr->packet)->th_seq;
		data_len = curr->packet_size - TCP_DATA_START(curr->packet);
		if ((curr_seq + data_len) == incoming_ack && (curr->num_transmits==1)) {
			gettimeofday(&curr_time,NULL);
			double curr_time_total = (double)curr_time.tv_sec + 
									 (curr_time.tv_usec/1000000.0);
			double start_time_total = (double)curr->tstart.tv_sec + 
									  (curr->tstart.tv_usec/1000000.0);
			double sample_rtt = (curr_time_total - start_time_total)*1000;
			if (ctx->estimated_rtt==0) {
				/* Didn't have an estimated RTT before. Use something conservative */
				ctx->estimated_rtt = 2*sample_rtt;
			}
			else {
				ctx->estimated_rtt = ((1.0-ALPHA)*sample_rtt) + 
									 (ALPHA*ctx->estimated_rtt);
				ctx->devrtt = (1.0 - BETA)*ctx->devrtt + 
						BETA*(fabs(sample_rtt - ctx->estimated_rtt));
			}	
			ctx->rto = 3.0*ctx->estimated_rtt + 4.0*ctx->devrtt;
			our_dprintf("\nSample RTT:%f ms  Estimated RTT: %f ms DEVRTT:%f ms RTO:%f ms\n\n",
						 sample_rtt, ctx->estimated_rtt, ctx->devrtt, ctx->rto);
			break;	
		}
		curr = curr->next;
	}
	
}


/* ***************************************************
 * Function: remove_until_seq
 * ***************************************************
 * When we receive an ACK, this is the function we call to
 * remove all entries up to the new/updated bottom of the 
 * send window.
 */
static void remove_until_seq(context_t* ctx)
{
	struct window* win = &(ctx->sbuffer);
	assert((win->start->next!=NULL) || !DEBUG);
	assert((win->num_entries>0) || !DEBUG);
	while (win->num_entries>0){
		struct p_entry* victim = win->start->next;
		tcp_seq sequence = ((struct tcphdr*)(victim->packet))->th_seq;
		if (sequence>=ctx->seq_base) break;
		our_dprintf("Removing Seq: %d from sender's buffer\n",
					 ((struct tcphdr*)victim->packet)->th_seq);
		if (victim==win->end) {
			win->end = win->start;
		}
		win->start->next = win->start->next->next;
		win->num_entries-=1;
		free(victim->packet);
		free(victim);
	}
}


/* ***************************************************
 * Function: send_ack
 * ***************************************************
 * Small helper function for preparing an ACK packet and sending
 * it to the network.   
 */
static void send_ack(mysocket_t sd, context_t* ctx)
{
	struct tcphdr *ack_pkt = (struct tcphdr *)calloc(1, (TH_MIN_OFFSET*sizeof(uint32_t)));
	ack_pkt->th_flags = TH_ACK;
	ack_pkt->th_ack = ctx->last_ack_sent;
	ack_pkt->th_seq = ctx->nxt_seq_num;
	our_dprintf("Sending ack: %d with seq: %d\n", ack_pkt->th_ack, ack_pkt->th_seq);	
	ack_pkt->th_win = RWIN;
	ack_pkt->th_off = TH_MIN_OFFSET;
	network_send(sd, ack_pkt, TH_MIN_OFFSET*sizeof(uint32_t));
	free(ack_pkt);	
}

/**********************************************************************/
/* our_dprintf
 *
 * Send a formatted message to stdout.
 * 
 * format               A printf-style format string.
 *
 * This function is equivalent to a printf, but may be
 * changed to log errors to a file if desired.
 *
 * Calls to this function are generated by the dprintf amd
 * dperror macros in transport.h
 */
void our_dprintf(const char *format,...)
{
    if (DEBUG){
	    va_list argptr;
	    char buffer[1024];
	
	    assert(format);
	    va_start(argptr, format);
	    vsnprintf(buffer, sizeof(buffer), format, argptr);
	    va_end(argptr);
	    fputs(buffer, stdout);
	    fflush(stdout);
    }
}

