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
//#include <sys/time.h>
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

int	send_network_msg(int sock, struct in6_addr *dest, int type);	// Local procedures
int	find_live_node(struct in6_addr *src);
int	add_live_node(struct in6_addr *src);
void	update_my_ip_details();

#define HOSTNAME_LEN	12					//Max supported host name length including null

struct net {							// Network descriptior
	struct in6_addr address;				// IPv6 Address
	int state;						// Overall node state
	int to;							// State of messages to node
	int from;						// State of messages from node
	char name[HOSTNAME_LEN];				// Node hostname
	struct in_addr addripv4;				// IPv4 address
	};

static char my_hostname[HOSTNAME_LEN];				// My hostname
struct in_addr my_ipv4_addr;					// & IPv4 address
static struct net other_nodes[NO_NETWORKS];			// Main network descriptor
static fd_set readfds;						// Network file descriptor

static unsigned int protocol_port = 5359;			// IPv6 multicast port & address
static struct in6_addr multicast_address;
#define MULTICAST "ff02::cca6:c0f9:e182:5359"
#define SIN_LEN 16
#define SIN4_LEN 8

const unsigned char zeros[SIN_LEN] = {0};			// IPv6 addresses
const unsigned char ones[SIN_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define MSG_TYPE_PING	44
#define MSG_TYPE_REPLY	45
#define MSG_TYPE_ECHO	46
#define MSG_VERSION	1

struct test_msg {						// Test message format
	char type;
	char version;
	char hops;
	char total_hops;
	struct in6_addr dest;
	char src_name[HOSTNAME_LEN];
	struct in_addr src_addripv4;
	};

//int nodns = 0, af = 3, request_prefix_delegation = 0;




//
//	Initialise network information
//
int	initialise_network() {
    int ifindex, rc = -1;
    struct ipv6_mreq mreq;
    int sock = -1;
    int one = 1, zero = 0;
    const int ds = 0xc0;        			// CS6 - Network Control

    struct sockaddr_in6 sin6;

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

    update_my_ip_details();					// Update local host name & Ip address

    return(sock);

ERRORBLOCK(die);
    printf("Error %d errno(%d)\n", rc, errno);
    close(sock);
ENDERROR;
    return(-1);
}

//
//	Wait on message or timer, whatever comes first
//
void	wait_on_network_timers(int sock, struct timer_list *timers) {
    int rc;
    struct timeval *wait;
								// attempt read on socket
    FD_SET(sock, &readfds);					// timeout on next timer
    wait = next_timers(timers);
    printf("Wait on read or timeout %llds\n", (long long)wait->tv_sec);
    rc = select(sock + 1, &readfds, NULL, NULL, wait);
    ERRORCHECK( (rc < 0) && (errno != EINTR), "Message wait on socket error\n", EndError);

ENDERROR;
}

//
//	Check live networks by sending out messages
//
int	check_live_nodes(int sock) {
    int i, rc, sent;

    update_my_ip_details();					// Update local host name & Ip address
    sent = 0;
    i = -1;
    rc = send_network_msg(sock, &multicast_address , MSG_TYPE_ECHO); // send out a broadcast message to identify networks
    ERRORCHECK( rc < 0,  "Broadcast send error\n", SendError);


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
            	}
	        rc = send_network_msg(sock, &other_nodes[i].address, MSG_TYPE_PING); // send out a specific message to this node
	        ERRORCHECK( rc < 0, "Network send error\n", SendError);
	        other_nodes[i].to = MSG_STATE_SENT;			// mark this node as having send a message
                other_nodes[i].from = MSG_STATE_UNKNOWN;		// and received status unknown
		sent = 1;
	    }
	}
    }
ERRORBLOCK(SendError);
    printf("Send error: Node %d, send error %d errno(%d)\n", i, rc, errno);
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

    printf("\nNetwork Status at %s (%s)\n", my_hostname, addr_string);

    for (i=0; i < NO_NETWORKS; i++) {				// For each of the networks
	if( memcmp(&other_nodes[i].address, &zeros, SIN_LEN) != 0) { // if an address is defined
            inet_ntop(AF_INET6, &other_nodes[i].address, (char *)&addr_string, 40);
            inet_ntop(AF_INET, &other_nodes[i].addripv4, (char *)&ipv4_string, 40);
	    printf("Node %d Address %s Name %s IPv4 %sstate:to:from %2d %2d %2d  ", i, addr_string, other_nodes[i].name, ipv4_string, other_nodes[i].state, other_nodes[i].to, other_nodes[i].from);
	}
    }
    printf("\n");
}

//
//	Check if a Network Message is available
//
int	check_network_msg(int sock) {

	return(FD_ISSET(sock, &readfds));
}

#define	MAX_BUFFER 100						//Maximum  network buffer size
//
//	Handle Network Message
//
void	handle_network_msg(int sock) {
    char buf[MAX_BUFFER];					// Full buffer
    struct test_msg *message = (struct test_msg*)&buf;		// Our Test Msg mapped over full buffer
    struct iovec iovec;
    struct msghdr msg;
    struct sockaddr_in6 sin6;
    int rc, node;

    memset(&msg, 0, sizeof(msg));				//Initialise message header
    iovec.iov_base = buf;
    iovec.iov_len = MAX_BUFFER;
    msg.msg_name = &sin6;					// ??
    msg.msg_namelen = sizeof(sin6);				// ??
    msg.msg_iov = &iovec;
    msg.msg_iovlen = 1;

    rc = recvmsg(sock, &msg, 0);
    ERRORCHECK( rc < 0, "Read message failed\n", EndError);

    ERRORCHECK( rc < 2, "Truncated packet\n", EndError);
    ERRORCHECK( (rc != sizeof(struct test_msg)), "Ill formed packet\n", EndError);
    ERRORCHECK( (message->type != MSG_TYPE_ECHO) && \
		(message->type != MSG_TYPE_PING) && \
		(message->type != MSG_TYPE_REPLY), "Unrecognised message type\n", EndError);
    ERRORCHECK( (message->version != MSG_VERSION), "Incompatible message version\n", EndError);

    if (memcmp(&message->dest, &multicast_address, SIN_LEN) == 0) { //if message was broadcast
	printf("Broadcast message received\n");
	node = add_live_node(&sin6.sin6_addr);			// Add the source as a valid node
	ERRORCHECK( node < 0, "Network node error\n", EndError);
    } else {
	node = find_live_node(&sin6.sin6_addr);			//
	ERRORCHECK( node < 0, "Network node unknown\n", EndError);

	switch (message->type) {				// Handle different message types
	case MSG_TYPE_REPLY:
            printf("Reply message received\n");
	    other_nodes[node].to = MSG_STATE_OK;		// Reply received - to stae is OK
	    other_nodes[node].state = NET_STATE_UP;		// Set link status UP
	    break;
	case MSG_TYPE_PING:
	    printf("Ping message received\n");
	    other_nodes[node].from = MSG_STATE_RECEIVED;	// Ping request
	    rc = send_network_msg( sock, &sin6.sin6_addr, MSG_TYPE_REPLY);	// Send reply
	    if (rc > 0) { 
		other_nodes[node].from = MSG_STATE_OK;		// and note as such
		other_nodes[node].state = NET_STATE_UP;		// Set link status UP
	    }
	    break;
	default:
	    printf("Neither Ping nor Replay received\n");
	}
    }
    memcpy(other_nodes[node].name, message->src_name, HOSTNAME_LEN);	// Maintain Hostname and  allocated IPv4 Address
    memcpy(&other_nodes[node].addripv4, &message->src_addripv4, SIN4_LEN);
ENDERROR;
}


//
//	Send a network message
//
int	send_network_msg(int sock, struct in6_addr *dest, int type ) {
    int rc;
    struct iovec iovec[2];
    struct msghdr msg;
    struct sockaddr_in6 sin6;
    struct test_msg message;
    char addr_string[40];

    inet_ntop(AF_INET6, dest, (char *)&addr_string, 40);
    printf("Send message - Socket %d Address %s Type %d\n", sock, addr_string, type);

    memset(&message, 0, sizeof(message));
    message.type = type;
    message.version = MSG_VERSION;
    message.hops = 0;
    message.total_hops = 0;
    memcpy(&message.dest, dest, SIN_LEN);
    memcpy(message.src_name, my_hostname, HOSTNAME_LEN);	// Include Hostname and  allocated IPv4 Address
    memcpy(&message.src_addripv4, &my_ipv4_addr, SIN4_LEN);

    iovec[0].iov_base = (void *)&message;
    iovec[0].iov_len = sizeof(message);
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

    rc = sendmsg( sock, &msg, 0);
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
    other_nodes[node].state = NET_STATE_UP;			// Set link and message states
    other_nodes[node].to = MSG_STATE_UNKNOWN;
    other_nodes[node].from = MSG_STATE_UNKNOWN;
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
    printf("Errno (%d)\n", errno);
    close(fd);
ENDERROR;
}
