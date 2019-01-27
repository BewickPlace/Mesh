/* Copyright (c) 2014 - by John Chandler

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
//#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
//#include <signal.h>
//#include <assert.h>
#include <fcntl.h>
#include <sys/time.h>
//#include <time.h>
//#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <wiringPi.h>
#include "errorcheck.h"
#include "timers.h"
#include "networks.h"

int	send_network_msg(struct in6_addr *dest, int type, char *payload, int payload_len, unsigned int seen, unsigned int payload_seq);	// Local procedures
int	find_live_node(struct in6_addr *src);
int	add_live_node(struct in6_addr *src);
void	update_my_ip_details();
void	cancel_reply_timer();

#define	MAX_BUFFER 200						// Maximum  network buffer size

struct net {							// Network descriptior
	struct in6_addr address;				// IPv6 Address
	int state;						// Overall node state
	int to;							// State of messages to node
	int from;						// State of messages from node
	char name[HOSTNAME_LEN];				// Node hostname
	struct in_addr addripv4;				// IPv4 address
	unsigned char to_seq;					// Payload sequence number ~ to
	unsigned char from_seq;					// Payload sequence number ~ from
								// Network statistics
	unsigned int tx;					// Packets Transmitted
	unsigned int rx;					// Packets Received
	unsigned int ping_sent;					// Pings sent
	unsigned int ping_seen;					// Pings seen (by remote node)
	unsigned int reply_seen;				// Replies seen
	unsigned int reply_tx;					// Replies transmitted (by this node)
	unsigned int payload_recv;				// Payloads received
	unsigned int payload_err;				// Payload sequence error
	};

static int netsock;						// network socket
static void (*link_up_callback)(char *name), (*link_down_callback)(char *name);		// Link callback functions
static char my_hostname[HOSTNAME_LEN];				// My hostname
struct in_addr my_ipv4_addr;					// & IPv4 address
static struct net other_nodes[NO_NETWORKS];			// Main network descriptor
static fd_set readfds;						// Network file descriptor

static unsigned int protocol_port = 5350;			// IPv6 multicast port & address
static struct in6_addr multicast_address;
#define MULTICAST "ff02::cca6:c0f9:e182:5350"
#define SIN_LEN 16
#define SIN4_LEN 4

const unsigned char zeros[SIN_LEN] = {0};			// IPv6 addresses
const unsigned char ones[SIN_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define MSG_TYPE_PING	44
#define MSG_TYPE_REPLY	45
#define MSG_TYPE_ECHO	46
#define MSG_TYPE_ECHO_REPLY 47
#define MSG_TYPE_PAYLOAD 50
#define MSG_VERSION	3

struct test_msg {						// Test message format
	char type;
	char version;
	struct in6_addr dest;
	char src_name[HOSTNAME_LEN];
	struct in_addr src_addripv4;
	unsigned int seen;
	unsigned int payload_seq;
	};

//
//	Initialise network information
//
int	initialise_network(int max_payload_len, void (*up_callback)(char *name), void (*down_callback)(char *name)) {
    int ifindex, rc = -1;
    struct ipv6_mreq mreq;
    int sock = -1;
    int one = 1, zero = 0;
    const int ds = 0xc0;        			// CS6 - Network Control
    struct test_msg message;

    struct sockaddr_in6 sin6;

    link_up_callback = NULL;			// Set Link status callbacks
    link_up_callback = up_callback;			// Set Link status callbacks
    link_down_callback = down_callback;

    ERRORCHECK( MAX_BUFFER < (sizeof(message) + max_payload_len), "Network buffer too small", small);

    FD_ZERO(&readfds);

    ifindex = if_nametoindex("wlan0");			// Use the wireless network interface
    ERRORCHECK( ifindex <=0, "uknown interface type", die);

    rc = inet_pton(AF_INET6, MULTICAST, &multicast_address);
    ERRORCHECK( rc < 0, "Protocol Group error", die);

    sock = socket(PF_INET6, SOCK_DGRAM, 0);
    ERRORCHECK( sock < 0, "socket error 1", EndError);

    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    ERRORCHECK( rc < 0, "socket error 2", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &zero, sizeof(zero));
    ERRORCHECK( rc < 0, "socket error 3", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &one, sizeof(one));
    ERRORCHECK( rc < 0, "socket error 3", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
    ERRORCHECK( rc < 0, "socket error 4", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, &ds, sizeof(ds));
    ERRORCHECK( rc < 0, "socket error 5", die);

    rc = fcntl(sock, F_GETFD, 0);
    ERRORCHECK( rc < 0, "fcntl error 1", die);

    rc = fcntl(sock, F_SETFD, rc | FD_CLOEXEC);
    ERRORCHECK( rc < 0, "fcntl error 2", die);

    rc = fcntl(sock, F_GETFL, 0);
    ERRORCHECK( rc < 0, "fcntl error 3", die);

    rc = fcntl(sock, F_SETFL, (rc | O_NONBLOCK));
    ERRORCHECK( rc < 0, "fcntl error 1", die);

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(protocol_port);
    rc = bind(sock, (struct sockaddr*)&sin6, sizeof(sin6));
    ERRORCHECK( rc < 0, "Bind error ", die);

    memset(&mreq, 0, sizeof(mreq));
    memcpy(&mreq.ipv6mr_multiaddr, &multicast_address, SIN_LEN);
    mreq.ipv6mr_interface = ifindex;
    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&mreq, sizeof(mreq));
    ERRORCHECK( rc<0, "set socket error", die);

    netsock = sock;						// Save network socket
    update_my_ip_details();					// Update local host name & Ip address

    return(sock);


ERRORBLOCK(small);
    warn("Message length: %d",sizeof(message));
    warn("Payload length: %d",max_payload_len);

ERRORBLOCK(die);
    warn("Error %d errno(%d)", rc, errno);
    close(sock);
ENDERROR;
    netsock = -1;						// No network socket
    return(-1);
}

//
//	network Closedown & tidy up
//
void	network_CLOSE() {

    close(netsock);
}

//
//	Wait on message or timer, whatever comes first
//
void	wait_on_network_timers() {
    int rc;
    struct timeval *wait;
    FD_ZERO(&readfds);
    wait = next_timers();
    debug(DEBUG_DETAIL, "Wait on read or timeout %lds\n", wait->tv_sec);
    if (wait->tv_sec > 0L) {					// As long as next timer not expired
								// attempt read on socket
	FD_SET(netsock, &readfds);					// timeout on next timer
	rc = select(netsock + 1, &readfds, NULL, NULL, wait);
	ERRORCHECK( (rc < 0) && (errno != EINTR), "Message wait on socket error", EndError);
	adjust_timers();
    }

ENDERROR;
}

static int network_warn = 0;
//
//	Broadcast to all nodes
//
void	broadcast_network() {
    int i, rc;

    i = -1;
    rc = send_network_msg(&multicast_address , MSG_TYPE_ECHO, NULL, 0, 0, 0); // send out a broadcast message to identify networks
    if (rc < 0) { goto SendError; }
    network_warn = 0;

ERRORBLOCK(SendError);
    if (network_warn == 0) {
	warn("Broadcast send error: Node %d, send error %d errno(%d)", i, rc, errno);
	network_warn = 1;
    }
ENDERROR;
}

//
//	Check live networks by sending out messages
//
int	check_live_nodes() {
    int i, rc, sent;
    char ipv4_string[40];

    update_my_ip_details();					// Update local host name & Ip address
    sent = 0;

    for (i=0; i < NO_NETWORKS; i++) {				// For each of the networks
	if (memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) { // if an address is defined
								// Check to determine correct network status
            if (other_nodes[i].state == NET_STATE_DOWN) {	// if has been down for an entire period
	   	other_nodes[i].state = NET_STATE_UNKNOWN;	// set state unknow and
                memcpy(&other_nodes[i].address, &zeros, SIN_LEN); // Remove address
                memcpy(&other_nodes[i].name, &zeros, HOSTNAME_LEN); // ... name
                memcpy(&other_nodes[i].addripv4, &zeros, SIN4_LEN); // ... old IPv4 address
	    } else {
		if ((other_nodes[i].to == MSG_STATE_FAILED) &&	// if both to and from states are questionable
		    (other_nodes[i].from == MSG_STATE_UNKNOWN)) {
		    other_nodes[i].state = NET_STATE_DOWN;	// Mark network as likely to be down
		    inet_ntop(AF_INET, &other_nodes[i].addripv4, (char *)&ipv4_string, 40);
		    debug(DEBUG_ESSENTIAL, "Link DOWN to node: %s (%s)\n", other_nodes[i].name, ipv4_string);
		    if (link_down_callback != NULL) link_down_callback(other_nodes[i].name);	// run callback if defined
            	}
	        rc = send_network_msg(&other_nodes[i].address, MSG_TYPE_PING, NULL, 0, 0, 0); // send out a specific message to this node
		if (rc < 0) { warn("PING send error: Node %d, send error %d errno(%d)", i, rc, errno); }
	        other_nodes[i].to = MSG_STATE_SENT;			// mark this node as having send a message
                other_nodes[i].from = MSG_STATE_UNKNOWN;		// and received status unknown

		other_nodes[i].tx++;
		other_nodes[i].ping_sent++;
		sent = 1;
	    }
	}
    }
    return(sent);
}



//
//	Expire those networks where a reply to a sent msg is outstanding
//
void	expire_live_nodes() {
    int i;
    int rc;

    for (i=0; i < NO_NETWORKS; i++) {				// For each of the networks
	if ((memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) && // if an address is defined
	    (other_nodes[i].state != NET_STATE_DOWN) &&
	    ((other_nodes[i].to == MSG_STATE_SENT) | (other_nodes[i].to == MSG_STATE_FAILED))) {

	    other_nodes[i].to = MSG_STATE_FAILED;		// mark this node as failed
	    rc = send_network_msg(&other_nodes[i].address, MSG_TYPE_PING, NULL, 0, 0, 0); // Re-Ping failed node
	    if (rc < 0) { warn("Re-PING send error: Node %d, send error %d errno(%d)", i, rc, errno); }
	    other_nodes[i].tx++;
	    other_nodes[i].ping_sent++;
	    add_timer(TIMER_REPLY, 3);
	    debug(DEBUG_TRACE, "Link to %s timed out, retry ping\n", other_nodes[i].name);
	}
    }
}



//
//	Display the current status of known other nodes
//
void	display_live_network() {
    int i;
    char addr_string[40];
    char ipv4_string[40];

    inet_ntop(AF_INET, &my_ipv4_addr, (char *)&addr_string, 40);
    debug(DEBUG_DETAIL, "Network Status at %s (%s)\n", my_hostname, addr_string);

    for (i=0; i < NO_NETWORKS; i++) {				// For each of the networks
	if( memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) { // if an address is defined
            inet_ntop(AF_INET6, &other_nodes[i].address, (char *)&addr_string, 40);
            inet_ntop(AF_INET, &other_nodes[i].addripv4, (char *)&ipv4_string, 40);
	    debug(DEBUG_DETAIL, "Node %d Address %s of %-12s (%s) state:to:from %2d %2d %2d Payload:%3d:%3d\n",
			i, addr_string, other_nodes[i].name, ipv4_string, other_nodes[i].state, other_nodes[i].to, other_nodes[i].from,
			other_nodes[i].to_seq, other_nodes[i].from_seq);
	}
    }
}

//
//	Report Network Statistics
//
void	report_network_stats() {
    int i;
    int ping_rate;
    int ping_err;
    int reply_err;
    int payload_rate;

    for (i=0; i < NO_NETWORKS; i++) {					// For each of the networks
	if( memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) { 	// if an address is defined
									// Report statistics
	    other_nodes[i].payload_recv = other_nodes[i].payload_recv + other_nodes[i].payload_err;	// add back in missed packets to total
													// pings already include missed replies
	    ping_err =  (other_nodes[i].ping_sent - other_nodes[i].ping_seen);
	    reply_err = (other_nodes[i].ping_seen - other_nodes[i].reply_seen);
	    ping_rate = ((ping_err + reply_err) * 100) / other_nodes[i].ping_sent;
	    payload_rate = (other_nodes[i].payload_err * 100)/other_nodes[i].payload_recv;
	    if ((ping_rate > 1) | (payload_rate > 1)) {
		debug(DEBUG_ESSENTIAL, "Network Stats %-12s, tx:rx[%d:%d] Ping:[%d of %d] Reply[%d of %d] Pay:[%d of %d]\n",
		other_nodes[i].name,
		other_nodes[i].tx,
		other_nodes[i].rx,
		ping_err,
		other_nodes[i].ping_sent,
		reply_err,
		other_nodes[i].ping_seen,
		other_nodes[i].payload_err,
		other_nodes[i].payload_recv);
	    } else {
		debug(DEBUG_ESSENTIAL, "Network Stats %-12s, tx:rx[%d:%d] Ping:[%d of %d] Reply[%d of %d] Pay:[%d of %d]\n",
		other_nodes[i].name,
		other_nodes[i].tx,
		other_nodes[i].rx,
		ping_err,
		other_nodes[i].ping_sent,
		reply_err,
		other_nodes[i].ping_seen,
		other_nodes[i].payload_err,
		other_nodes[i].payload_recv);
	    }
	    other_nodes[i].tx = 0;				// Reset Network statistics
	    other_nodes[i].rx = 0;
	    other_nodes[i].ping_sent = 0;
	    other_nodes[i].ping_seen = 0;
	    other_nodes[i].reply_seen = 0;
	    other_nodes[i].reply_tx = 0;
	    other_nodes[i].payload_recv = 0;
	    other_nodes[i].payload_err = 0;
	}
    }
}

//
//	Check if a Network Message is available
//
int	check_network_msg() {

 	return(FD_ISSET(netsock, &readfds));
}

//
//	Handle Network Message
//
void	handle_network_msg(char *node_name, char *payload, int *payload_len) {
    char full_message[MAX_BUFFER];				// Full buffer
    struct test_msg *message = (struct test_msg*)full_message;	// Our Test Msg mapped over full buffer
    struct iovec iovec;
    struct msghdr msg;
    struct sockaddr_in6 sin6;
    int rc, node;
    char ipv4_string[40];
    int error_count;

    memset(&msg, 0, sizeof(msg));				//Initialise message header
    iovec.iov_base = full_message;
    iovec.iov_len = MAX_BUFFER;
    msg.msg_name = &sin6;					// ??
    msg.msg_namelen = sizeof(sin6);				// ??
    msg.msg_iov = &iovec;
    msg.msg_iovlen = 1;
    *payload_len = 0;						// No payload yet

    rc = recvmsg(netsock, &msg, 0);
    if ((rc < 0) && (errno== EAGAIN)) { goto EndAgain; }	//  If EAGAIN, skip to end without Warning message
    ERRORCHECK( rc < 0, "Read message failed", ReadError);

    ERRORCHECK( rc < 2, "Truncated packet", EndError);
    *payload_len = rc - sizeof(struct test_msg);

    ERRORCHECK( (message->version != MSG_VERSION), "Incompatible message version", EndError);
//    ERRORCHECK( (rc != sizeof(struct test_msg)), "Ill formed packet\n", EndError);
    if ((message->type == MSG_TYPE_PAYLOAD) &&			// if this is a payload packet
	(*payload_len != 0)) {

	memcpy(payload, &full_message[sizeof(struct test_msg)], *payload_len);	// copy it back to the users byffer
	memcpy(node_name, message->src_name, HOSTNAME_LEN);	// Return Hostname

	node = get_active_node(node_name);			// find which node this is
	ERRORCHECK(node < 0, "Unidentified payload source", EndError);

	other_nodes[node].from_seq++;
	other_nodes[node].rx++;
	other_nodes[node].payload_recv++;
	if (other_nodes[node].from_seq != message->payload_seq) {
	    debug(DEBUG_TRACE, "Payload from %s received out of sequence [%d:%d]\n", node_name, message->payload_seq, other_nodes[node].from_seq);
	    error_count = message->payload_seq - other_nodes[node].from_seq;	// Calculate number of missed packets
	    other_nodes[node].from_seq = message->payload_seq;			// Reset next expected sequence number
	    other_nodes[node].payload_err =					// Maintain sequence error count
		other_nodes[node].payload_err + (error_count < 0 ? 1 : error_count); // add missed packets or for advance packets just 1
	}
	debug(DEBUG_DETAIL,"Payload from %s of type %d seq [%3d]\n", node_name, *(int *)payload, other_nodes[node].from_seq);
	return;							// and return
    }

    node = find_live_node(&sin6.sin6_addr);			// Check if this node is known

    switch (message->type) {					// Handle different message types
    case MSG_TYPE_ECHO:
    case MSG_TYPE_ECHO_REPLY:
	debug(DEBUG_DETAIL, "Broadcast message received\n");	// Manage creation of live node

	if (node < 0) {						// if unknown source
	    node = add_live_node(&sin6.sin6_addr);		// Add the source as a valid node

	    other_nodes[node].rx++;
	    if (message->type == MSG_TYPE_ECHO) {		// and first Broadcast received
		rc = send_network_msg(&sin6.sin6_addr, MSG_TYPE_ECHO_REPLY, NULL, 0, 0, 0); // Send specific broadcast response
		if (rc < 0) { warn("ECHO REPLY send error: Node %d, send error %d errno(%d)", node, rc, errno); }
		other_nodes[node].tx++;
	    }
	    add_timer(TIMER_PING, 1);				// initiate PINGs
	} else if (other_nodes[node].state == NET_STATE_DOWN) {
	    other_nodes[node].state = NET_STATE_UNKNOWN;	// Set link and message states as though new
	    other_nodes[node].to = MSG_STATE_UNKNOWN;
	    other_nodes[node].from = MSG_STATE_UNKNOWN;
	    other_nodes[node].to_seq = 0;			// Reset Payload to/from sequence numbers
	    other_nodes[node].from_seq = 0;
	    other_nodes[node].tx = 0;				// Reset Network statistics
	    other_nodes[node].rx = 0;
	    other_nodes[node].ping_sent = 0;
	    other_nodes[node].ping_seen = 0;
	    other_nodes[node].reply_seen = 0;
	    other_nodes[node].reply_tx = 0;
	    other_nodes[node].payload_recv = 0;
	    other_nodes[node].payload_err = 0;
	}
	break;
    case MSG_TYPE_REPLY:
	ERRORCHECK( node < 0, "Network node unknown (REPLY)", EndError);
	other_nodes[node].rx++;
	other_nodes[node].ping_seen = message->seen;
	other_nodes[node].reply_seen++;

	debug(DEBUG_DETAIL, "Reply message received\n");
	other_nodes[node].to = MSG_STATE_OK;			// Reply received - to stae is OK
	if (other_nodes[node].state != NET_STATE_UP) {		// Link state changes
	    other_nodes[node].state = NET_STATE_UP;		// Set link status UP
            inet_ntop(AF_INET, &message->src_addripv4, (char *)&ipv4_string, 40);
	    debug(DEBUG_ESSENTIAL, "Link UP   to node: %s (%s)\n", message->src_name, ipv4_string);
	    if (link_up_callback != NULL) link_up_callback(message->src_name);	// run callback if defined
	}
	cancel_reply_timer();					// Cancel reply timer if all now received
	break;
    case MSG_TYPE_PING:
	if (node < 0) goto EndError;
	other_nodes[node].rx++;
	other_nodes[node].reply_tx++;;

	debug(DEBUG_DETAIL, "Ping message received\n");
	other_nodes[node].from = MSG_STATE_RECEIVED;		// Ping request
	rc = send_network_msg(&sin6.sin6_addr, MSG_TYPE_REPLY, NULL, 0, other_nodes[node].reply_tx,0);	// Send reply
	if (rc < 0) { warn("REPLY send error: Node %d, send error %d errno(%d)", node, rc, errno); }
	else {other_nodes[node].from = MSG_STATE_OK; }		// and note as such
	other_nodes[node].tx++;
	break;
    default:
	warn("Neither Ping nor Replay received");
    }
    memcpy(other_nodes[node].name, message->src_name, HOSTNAME_LEN);	// Maintain Hostname and  allocated IPv4 Address
    memcpy(&other_nodes[node].addripv4, &message->src_addripv4, SIN4_LEN);
ERRORBLOCK(ReadError);
    warn("Read error: error %d errno(%d)", rc, errno);
    *payload_len = 0;						// Ensure No payload
ERRORBLOCK(EndAgain);
    debug(DEBUG_ESSENTIAL, "EAGAIN error %d:%d\n", rc, errno);
ENDERROR;
}


//
//	Send a network message
//
int	send_network_msg(struct in6_addr *dest, int type, char *payload, int payload_len, unsigned int seen, unsigned int payload_seq) {
    int rc;
    int retry;
    struct iovec iovec[2];
    struct msghdr msg;
    struct sockaddr_in6 sin6;
    char addr_string[40];
    char full_message[MAX_BUFFER];				// Full buffer
    struct test_msg *message = (struct test_msg*)full_message;	// Our Test Msg mapped over full buffer

    if ((inet_ntop(AF_INET6, dest, (char *)&addr_string, 40)) == NULL) {return(-1);}
    debug(DEBUG_DETAIL, "Send message - Address %s Type %d\n", addr_string, type);

    memset(message, 0, MAX_BUFFER);
    message->type = type;
    message->version = MSG_VERSION;
    memcpy(&message->dest, dest, SIN_LEN);
    memcpy(message->src_name, my_hostname, HOSTNAME_LEN);	// Include Hostname and  allocated IPv4 Address
    memcpy(&message->src_addripv4, &my_ipv4_addr, SIN4_LEN);
    message->seen = seen;					// undate with seen counter

    if (payload_len > 0) {					// and add payload if appropriate
	message->payload_seq = payload_seq;			// including sequence number
	memcpy(&full_message[sizeof(struct test_msg)], payload, payload_len);
    }

    iovec[0].iov_base = (void *)full_message;
    iovec[0].iov_len = sizeof(struct test_msg)+payload_len;
    iovec[1].iov_base = NULL;
    iovec[1].iov_len = 0;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(protocol_port);
    memcpy(&sin6.sin6_addr, dest, SIN_LEN);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iovec;
    msg.msg_iovlen = 2;
    msg.msg_name = &sin6;
    msg.msg_namelen = sizeof(sin6);

    for( retry = 0; retry < 40 ; retry++) {			// Loop for a Maxmimum of n retries
	rc = sendmsg( netsock, &msg, 0);

	if((rc < 0) &&						// If network error
	   ((errno == ENETUNREACH) ||				// Network Unreachable
	    (errno == EADDRNOTAVAIL))) {			// Address Mot Available
	    delay(100);						// delay for 100ms before we try again
	} else {
	    break;
	}
    }
    if (retry > 0) { warn("Network instability for %d msec", retry * 100); }
    return(rc);
}

//
//	Find live node if it exists
//
int	find_live_node(struct in6_addr *src) {
    int i;
    int found = -1;

    for(i = 0; i < NO_NETWORKS; i++) {				// Look at eacxh network
	if (memcmp(&other_nodes[i].address, src, SIN_LEN) == 0) {// and check if source is already recognised
	    found = i;
	    break;
	}
    }
    return( found );
}

//
//	Add a new node to the node list
//
int	add_live_node(struct in6_addr *src) {
    int node;

    node = find_live_node(src);					// Check node doesn't exist
    if (node >= 0) goto EndError;				// skip on if already found

    node = find_live_node((struct in6_addr*)&zeros);		// Find an empty node
    ERRORCHECK( node < 0, "Node list full!!", EndError);

    memcpy(&other_nodes[node].address, src, SIN_LEN);		// Record address &
    other_nodes[node].state = NET_STATE_UNKNOWN;		// Set link and message states
    other_nodes[node].to = MSG_STATE_UNKNOWN;
    other_nodes[node].from = MSG_STATE_UNKNOWN;
    other_nodes[node].to_seq = 0;				// Reset Payload to/from sequence numbers
    other_nodes[node].from_seq = 0;
    other_nodes[node].tx = 0;					// Reset Network statistics
    other_nodes[node].rx = 0;
    other_nodes[node].ping_sent = 0;
    other_nodes[node].ping_seen = 0;
    other_nodes[node].reply_seen = 0;
    other_nodes[node].reply_tx = 0;
    other_nodes[node].payload_recv = 0;
    other_nodes[node].payload_err = 0;
ENDERROR;
    return(node);
}

//
//	Get the Active nodes id from the name
//	find any node if name blank
//
int	get_active_node(char *name) {
    int node;
    int found = -1;

    for(node = 0; node < NO_NETWORKS; node++) {			// Look at each network
	if (other_nodes[node].state == NET_STATE_UP) {		// and check if marked as up
	    if ((strcmp(name, "") == 0) |			// If looing for any
		(strcmp(name, other_nodes[node].name) == 0)) {	// or looking for specific
		found = node;					// identify active node found
	    	break;
	    }
	}
    }
    return( found );
}
//
//	Find any Active nodes & return id
//
int	find_active_node() {
    return( get_active_node(""));
}
//
//	Update local host Name & IPv4 Address
//
void	update_my_ip_details() {
    int rc;
    int fd;							// Network file descriptor
    struct ifreq ifr;						// Interface request record


    rc = gethostname(my_hostname, HOSTNAME_LEN-1);		// Obtain local host name
    ERRORCHECK(rc < 0, "Invalid host name", EndError);

    my_hostname[HOSTNAME_LEN-1] = '\0';				// Endure name  string terminated

    fd = socket(AF_INET, SOCK_DGRAM, 0);			// find IPv4 socket
    ERRORCHECK(fd < 0, "IPv4 Socket error", ErrnoError);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);

    rc = ioctl(fd, SIOCGIFADDR, &ifr);
    if ((rc < 0) && (errno == EADDRNOTAVAIL)) { goto IOCTLError; }
    ERRORCHECK(rc < 0, "IO ctl Error", ErrnoError);
    close(fd);

    memcpy(&my_ipv4_addr, &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), SIN4_LEN);

ERRORBLOCK(ErrnoError);
    warn("Errno (%d)", errno);
    close(fd);
ERRORBLOCK(IOCTLError);
    close(fd);
ENDERROR;
}
//
//	Return a pointer to my name
//
char 	*my_name() {

    return(my_hostname);
}


//
//	Cancel Reply Timer if all now received
//
void	cancel_reply_timer() {
    int i;

    for(i=0; i < NO_NETWORKS; i++) {				// Check all valid nodes for outstanding reply
	if ((memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) &&
	    ((other_nodes[i].to == MSG_STATE_SENT) | (other_nodes[i].to == MSG_STATE_FAILED))) { goto EndError; } // if we find one - bottle out
    }
    cancel_timer(TIMER_REPLY);					// None found we can cancel the timer

ENDERROR;
}

//
//	Send Payload to connected Node
//
int	send_to_node(int node, char *payload, int payload_len) {
    int rc;
    rc = -1;
    ERRORCHECK(other_nodes[node].state != NET_STATE_UP, "Send Payload - link down", EndError);	// Check Link UP

    other_nodes[node].to_seq++;
    rc = send_network_msg(&other_nodes[node].address, MSG_TYPE_PAYLOAD, payload, payload_len, 0, other_nodes[node].to_seq); // send out a specific message to this node
    if (rc < 0) { goto SendError; }
    other_nodes[node].tx++;
    debug(DEBUG_DETAIL,"Payload to %s of type %d seq [%3d]\n", other_nodes[node].name, *(int *)payload, other_nodes[node].to_seq);

ERRORBLOCK(SendError);
    warn("Payload send error: Node %d, send error %d errno(%d)", node, rc, errno);

ENDERROR;
    return(rc);
}
