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

#include "errorcheck.h"
#include "timers.h"
#include "networks.h"

int	send_network_msg(struct in6_addr *dest, int type, time_t node_delay, char *payload, int payload_len);	// Local procedures
int	find_live_node(struct in6_addr *src);
int	add_live_node(struct in6_addr *src);
void	update_my_ip_details();
void	cancel_reply_timer(struct timer_list *timers);
void	handle_delay_calc(int node, struct timeval t1, struct timeval t2, struct timeval t3, struct timeval t4, time_t remote_delay);

#define HOSTNAME_LEN	14					// Max supported host name length including null
#define	MAX_BUFFER 200						// Maximum  network buffer size

struct net {							// Network descriptior
	struct in6_addr address;				// IPv6 Address
	int state;						// Overall node state
	int to;							// State of messages to node
	int from;						// State of messages from node
	char name[HOSTNAME_LEN];				// Node hostname
	struct in_addr addripv4;				// IPv4 address
	time_t delay;						// Round trip delay (ms)
	time_t remote_delay;					// Round trip delay (ms) seen by other node
	};

static int netsock;						// network socket
static void (*link_up_callback)(void), (*link_down_callback)(void);		// Link callback functions
static char my_hostname[HOSTNAME_LEN];				// My hostname
struct in_addr my_ipv4_addr;					// & IPv4 address
static struct net other_nodes[NO_NETWORKS];			// Main network descriptor
static fd_set readfds;						// Network file descriptor

static unsigned int protocol_port = 5359;			// IPv6 multicast port & address
static struct in6_addr multicast_address;
#define MULTICAST "ff02::cca6:c0f9:e182:5359"
#define SIN_LEN 16
#define SIN4_LEN 4

const unsigned char zeros[SIN_LEN] = {0};			// IPv6 addresses
const unsigned char ones[SIN_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define MSG_TYPE_PING	44
#define MSG_TYPE_REPLY	45
#define MSG_TYPE_ECHO	46
#define MSG_TYPE_PAYLOAD 50
#define MSG_VERSION	1

struct test_msg {						// Test message format
	char type;
	char version;
	char hops;
	char total_hops;
	struct in6_addr dest;
	char src_name[HOSTNAME_LEN];
	struct in_addr src_addripv4;
	struct timeval t1, t2, t3, t4;				// NTP delay calculation timestamps
	time_t  remotedelay;					// other nodes view of current delay
	};

//int nodns = 0, af = 3, request_prefix_delegation = 0;




//
//	Initialise network information
//
int	initialise_network(int max_payload_len, void (*up_callback)(void), void (*down_callback)(void)) {
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
    ERRORCHECK( ifindex <=0, "uknown interface type\n", die);

    rc = inet_pton(AF_INET6, MULTICAST, &multicast_address);
    ERRORCHECK( rc < 0, "Protocol Group error\n", die);

    sock = socket(PF_INET6, SOCK_DGRAM, 0);
    ERRORCHECK( sock < 0, "socket error 1\n", EndError);

    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    ERRORCHECK( rc < 0, "socket error 2\n", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &zero, sizeof(zero));
    ERRORCHECK( rc < 0, "socket error 3\n", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &one, sizeof(one));
    ERRORCHECK( rc < 0, "socket error 3\n", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
    ERRORCHECK( rc < 0, "socket error 4\n", die);

    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, &ds, sizeof(ds));
    ERRORCHECK( rc < 0, "socket error 5\n", die);

    rc = fcntl(sock, F_GETFD, 0);
    ERRORCHECK( rc < 0, "fcntl error 1\n", die);

    rc = fcntl(sock, F_SETFD, rc | FD_CLOEXEC);
    ERRORCHECK( rc < 0, "fcntl error 2\n", die);

    rc = fcntl(sock, F_GETFL, 0);
    ERRORCHECK( rc < 0, "fcntl error 3\n", die);

    rc = fcntl(sock, F_SETFL, (rc | O_NONBLOCK));
    ERRORCHECK( rc < 0, "fcntl error 1\n", die);

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(protocol_port);
    rc = bind(sock, (struct sockaddr*)&sin6, sizeof(sin6));
    ERRORCHECK( rc < 0, "Bind error \n", die);

    memset(&mreq, 0, sizeof(mreq));
    memcpy(&mreq.ipv6mr_multiaddr, &multicast_address, SIN_LEN);
    mreq.ipv6mr_interface = ifindex;
    rc = setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&mreq, sizeof(mreq));
    ERRORCHECK( rc<0, "set socket error\n", die);

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
//	Wait on message or timer, whatever comes first
//
void	wait_on_network_timers(struct timer_list *timers) {
    int rc;
    struct timeval *wait;
    wait = next_timers(timers);
    debug(DEBUG_DETAIL, "Wait on read or timeout %llds\n", (long long)wait->tv_sec);
    if (wait->tv_sec > 0L) {					// As long as next timer not expired
								// attempt read on socket
	FD_SET(netsock, &readfds);					// timeout on next timer
	rc = select(netsock + 1, &readfds, NULL, NULL, wait);
	ERRORCHECK( (rc < 0) && (errno != EINTR), "Message wait on socket error\n", EndError);
    }

ENDERROR;
}

//
//	Broadcast to all nodes
//
void	broadcast_network() {
    int i, rc;

    i = -1;
    rc = send_network_msg(&multicast_address , MSG_TYPE_ECHO, 0, NULL, 0); // send out a broadcast message to identify networks
    ERRORCHECK( rc < 0,  "Broadcast send error\n", SendError);

ERRORBLOCK(SendError);
    warn("Send error: Node %d, send error %d errno(%d)", i, rc, errno);
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
		    debug(DEBUG_TRACE, "Link DOWN to node: %s (%s)\n", other_nodes[i].name, ipv4_string);
		    if (link_down_callback != NULL) link_down_callback();	// run callback if defined
            	}
	        rc = send_network_msg(&other_nodes[i].address, MSG_TYPE_PING, 0, NULL, 0); // send out a specific message to this node
	        ERRORCHECK( rc < 0, "Network send error\n", SendError);
	        other_nodes[i].to = MSG_STATE_SENT;			// mark this node as having send a message
                other_nodes[i].from = MSG_STATE_UNKNOWN;		// and received status unknown
		sent = 1;
	    }
	}
    }
ERRORBLOCK(SendError);
    warn("Send error: Node %d, send error %d errno(%d)", i, rc, errno);
ENDERROR;
    return(sent);
}



//
//	Expire those networks where a reply to a sent msg is outstanding
//
void	expire_live_nodes() {
    int i;

    for (i=0; i < NO_NETWORKS; i++) {				// For each of the networks
	if ((memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) && // if an address is defined
	    (other_nodes[i].to == MSG_STATE_SENT)) {		// and awaiting outstanding reply
	    other_nodes[i].to = MSG_STATE_FAILED;		// mark this node as failed
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
	    debug(DEBUG_DETAIL, "Node %d Address %s of %-12s (%s) state:to:from %2d %2d %2d  delays %lld(l) %lld(r)\n",
			i, addr_string, other_nodes[i].name, ipv4_string, other_nodes[i].state, other_nodes[i].to, other_nodes[i].from,
		    	(long long)other_nodes[i].delay, (long long)other_nodes[i].remote_delay);
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
void	handle_network_msg(struct timer_list *timers, char *payload, int *payload_len) {
    char full_message[MAX_BUFFER];				// Full buffer
    struct test_msg *message = (struct test_msg*)full_message;	// Our Test Msg mapped over full buffer
    struct iovec iovec;
    struct msghdr msg;
    struct sockaddr_in6 sin6;
    int rc, node;
    char ipv4_string[40];

    memset(&msg, 0, sizeof(msg));				//Initialise message header
    iovec.iov_base = full_message;
    iovec.iov_len = MAX_BUFFER;
    msg.msg_name = &sin6;					// ??
    msg.msg_namelen = sizeof(sin6);				// ??
    msg.msg_iov = &iovec;
    msg.msg_iovlen = 1;

    rc = recvmsg(netsock, &msg, 0);
    ERRORCHECK( rc < 0, "Read message failed\n", EndError);

    ERRORCHECK( rc < 2, "Truncated packet\n", EndError);
    *payload_len = rc - sizeof(struct test_msg);

//    ERRORCHECK( (rc != sizeof(struct test_msg)), "Ill formed packet\n", EndError);
    if ((message->type == MSG_TYPE_PAYLOAD) &&			// if this is a payload packet
	(*payload_len != 0)) {
	memcpy(payload, &full_message[sizeof(struct test_msg)], *payload_len);	// copy it back to the users byffer
	return;							// and return
    }

    ERRORCHECK( (message->type != MSG_TYPE_ECHO) && \
		(message->type != MSG_TYPE_PING) && \
		(message->type != MSG_TYPE_REPLY), "Unrecognised message type\n", EndError);
    ERRORCHECK( (message->version != MSG_VERSION), "Incompatible message version\n", EndError);

    node = find_live_node(&sin6.sin6_addr);			// Check if this node is known
    if (node < 0) {						// if unknown
	node = add_live_node(&sin6.sin6_addr);			// Add the source as a valid node
	add_timer(timers, TIMER_PING, 1);			// initiate PINGs
    }
    ERRORCHECK( node < 0, "Network node unknown\n", EndError);

    switch (message->type) {					// Handle different message types
    case MSG_TYPE_ECHO:
	debug(DEBUG_DETAIL, "Broadcast message received\n");	// Nothing else to do as have added live  node
	break;
    case MSG_TYPE_REPLY:
	debug(DEBUG_DETAIL, "Reply message received\n");
	gettimeofday(&(message->t4), NULL);			// T4 - Received timestamp
	handle_delay_calc( node, message->t1, message->t2, message->t3, message->t4, message->remotedelay);
	other_nodes[node].to = MSG_STATE_OK;			// Reply received - to stae is OK
	if (other_nodes[node].state != NET_STATE_UP) {		// Link state changes
	    other_nodes[node].state = NET_STATE_UP;		// Set link status UP
            inet_ntop(AF_INET, &message->src_addripv4, (char *)&ipv4_string, 40);
	    debug(DEBUG_TRACE, "Link UP   to node: %s (%s)\n", message->src_name, ipv4_string);
	    if (link_up_callback != NULL) link_up_callback();	// run callback if defined
	}
	cancel_reply_timer(timers);				// Cancel reply timer if all now received
	break;
    case MSG_TYPE_PING:
	debug(DEBUG_DETAIL, "Ping message received\n");
	gettimeofday(&(message->t2), NULL);			// T2 - Received timestamp
	other_nodes[node].from = MSG_STATE_RECEIVED;		// Ping request
	rc = send_network_msg(&sin6.sin6_addr, MSG_TYPE_REPLY, other_nodes[node].delay, NULL, 0);	// Send reply - with our view of delay
	if (rc > 0) {
	    other_nodes[node].from = MSG_STATE_OK;		// and note as such
	}
	break;
    default:
	warn("Neither Ping nor Replay received");
    }
    memcpy(other_nodes[node].name, message->src_name, HOSTNAME_LEN);	// Maintain Hostname and  allocated IPv4 Address
    memcpy(&other_nodes[node].addripv4, &message->src_addripv4, SIN4_LEN);
ENDERROR;
}


//
//	Send a network message
//
int	send_network_msg(struct in6_addr *dest, int type, time_t node_delay, char *payload, int payload_len) {
    int rc;
    struct iovec iovec[2];
    struct msghdr msg;
    struct sockaddr_in6 sin6;
    char addr_string[40];
    char full_message[MAX_BUFFER];				// Full buffer
    struct test_msg *message = (struct test_msg*)full_message;	// Our Test Msg mapped over full buffer

    inet_ntop(AF_INET6, dest, (char *)&addr_string, 40);
    debug(DEBUG_DETAIL, "Send message - Address %s Type %d\n", addr_string, type);

    memset(message, 0, MAX_BUFFER);
    message->type = type;
    message->version = MSG_VERSION;
    message->hops = 0;
    message->total_hops = 0;
    memcpy(&message->dest, dest, SIN_LEN);
    memcpy(message->src_name, my_hostname, HOSTNAME_LEN);	// Include Hostname and  allocated IPv4 Address
    memcpy(&message->src_addripv4, &my_ipv4_addr, SIN4_LEN);

    if (type == MSG_TYPE_PING) {
	    gettimeofday(&(message->t1), NULL);			// T1 - Sent timestamp
    } else if (type == MSG_TYPE_REPLY) {
	    gettimeofday(&(message->t3), NULL);			// T3 - Sent timestamp
    }
    message->remotedelay = node_delay;				// include view of the delay
    if (payload_len > 0)					// and add payload if appropriate
	{ memcpy(&full_message[sizeof(struct test_msg)], payload, payload_len); }

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

    rc = sendmsg( netsock, &msg, 0);
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
    ERRORCHECK( node < 0, "Node list full!!\n", EndError);

    memcpy(&other_nodes[node].address, src, SIN_LEN);		// Record address &
    other_nodes[node].state = NET_STATE_UNKNOWN;		// Set link and message states
    other_nodes[node].to = MSG_STATE_UNKNOWN;
    other_nodes[node].from = MSG_STATE_UNKNOWN;
    other_nodes[node].delay = 0;
    other_nodes[node].remote_delay = 0;
ENDERROR;
    return(node);
}

//
//	Update local host Name & IPv4 Address
//
void	update_my_ip_details() {
    int rc;
    int fd;							// Network file descriptor
    struct ifreq ifr;						// Interface request record


    rc = gethostname(my_hostname, HOSTNAME_LEN-1);		// Obtain local host name
    ERRORCHECK(rc < 0, "Invalid host name\n", EndError);

    my_hostname[HOSTNAME_LEN-1] = '\0';				// Endure name  string terminated

    fd = socket(AF_INET, SOCK_DGRAM, 0);			// find IPv4 socket
    ERRORCHECK(fd < 0, "IPv4 Socket error\n", ErrnoError);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);

    rc = ioctl(fd, SIOCGIFADDR, &ifr);
    ERRORCHECK(rc < 0, "Invalid host name\n", ErrnoError);
    close(fd);

    memcpy(&my_ipv4_addr, &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), SIN4_LEN);

ERRORBLOCK(ErrnoError);
    warn("Errno (%d)", errno);
    close(fd);
ENDERROR;
}

//
//	Cancel Reply Timer if all now received
//
void	cancel_reply_timer(struct timer_list *timers) {
    int i;

    for(i=0; i < NO_NETWORKS; i++) {			// Check all valid nodes for outstanding reply
	if ((memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) &&
	    (other_nodes[i].to == MSG_STATE_SENT)) { goto EndError; } // if we find one - bottle out
    }
    cancel_timer(timers, TIMER_REPLY);				// None found we can cancel the timer

ENDERROR;
}

//
//	Send Payload to connected Node
//
int	send_to_node(int node, char *payload, int payload_len) {
    int rc;

    rc = -1;
    ERRORCHECK(other_nodes[node].state != NET_STATE_UP, "Send Payload - link down\n", EndError);	// Check Link UP

    rc = send_network_msg(&other_nodes[node].address, MSG_TYPE_PAYLOAD, 0, payload, payload_len); // send out a specific message to this node
    ERRORCHECK( rc < 0, "Network send error\n", EndError);

ENDERROR;
    return(rc);
}

//
//	Handle the delay calculation using NTP algorythm
//
//	delay = (T4-T1) - (T3-T2)
//

void	handle_delay_calc(int node, struct timeval t1, struct timeval t2, struct timeval t3, struct timeval t4, time_t remote_delay) {

    time_t delay;

    timersub(&t4, &t1, &t4);				// T4-T1
    timersub(&t3, &t2, &t3);				// T3-T1
    timersub(&t4, &t3, &t4);				// Delay in timeval

    delay = (t4.tv_usec);			// convert to ms
//    delay = (t4.tv_usec / 1000);			// convert to ms
//    delay = (t4.tv_sec * 1000) + delay;

    other_nodes[node].delay = delay;			// record the delay for this node
    other_nodes[node].remote_delay = remote_delay;	// and seen by the counterpart node
}
