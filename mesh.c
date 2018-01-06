/*
Copyright (c) 2014- by John Chandler

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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

//#include <unistd.h>
//#include <signal.h>
//#include <assert.h>
//#include <fcntl.h>
//#include <sys/time.h>
//#include <time.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/uio.h>
//#include <netinet/in.h>
//#include <netinet/ip.h>
//#include <arpa/inet.h>
//#include <net/if.h>

#include "errorcheck.h"
#include "timers.h"
#include "networks.h"

#define	OPMODE_MASTER	0				// Node operating modes
#define	OPMODE_SLAVE	1				//
static int	operating_mode = OPMODE_MASTER;		// Default operating mode (changed by command line)

void usage(char *progname) {
    printf("Usage: %s [options...]\n", progname);

    printf("\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");

    printf("\n");
    printf("Options:\n");
    printf("    -h, --help          show this help\n");
    printf("    -m, --master        Master operating mode\n");
    printf("    -s, --slave         Slave operating mode\n");

    printf("    -l, --log=FILE      redirect shairport's standard output to FILE\n");
    printf("                        If --error is not specified, it also redirects\n");
    printf("                        error output to FILE\n");
    printf("    -e, --error=FILE    redirect shairport's standard error output to FILE\n");

    printf("\n");
}

int parse_options(int argc, char **argv) {

    static struct option long_options[] = {
        {"help",    no_argument,        NULL, 'h'},
        {"master",  no_argument,        NULL, 'm'},
        {"slave",   no_argument,        NULL, 's'},

        {"log",     required_argument,  NULL, 'l'},
        {"error",   required_argument,  NULL, 'e'},
        {NULL, 0, NULL, 0}
    };
    int opt;

    while ((opt = getopt_long(argc, argv,
                              "+hmsvl:e:",
                              long_options, NULL)) > 0) {
        switch (opt) {
            default:
            case 'h':
                usage(argv[0]);
                exit(1);
            case 'm':
		operating_mode = OPMODE_MASTER;			// Set Node as Master
                break;
            case 's':
		operating_mode = OPMODE_SLAVE;			// Set Node as Slave
                break;
            case 'v':
                debuglev++;
                break;

            case 'l':
//                config.logfile = optarg;
                break;
            case 'e':
//                config.errfile = optarg;
                break;
        }
    }
    return optind;
}

//
//
//	Main procedure
//
//
int main(int argc, char **argv) {
    int shutdown = 0;					// Shutdown flag
    int protocol_socket;				// Network protocol socket
    struct timer_list timers;				// Timers

    parse_options(argc, argv);				// Parse command line parameters

    protocol_socket =  initialise_network();		// Initialise the network details
    initialise_timers(&timers);				// and set all timers

    if (operating_mode == OPMODE_MASTER) {		// Only Master nodes are responsible for broadcasting
	add_timer(&timers, TIMER_BROADCAST, 5);		// Set to refresh network in y seconds
    }

    while (!shutdown) {					// While NOT shutdown
	printf("====================\n");
	wait_on_network_timers(protocol_socket, &timers); // Wait for message or timer expiory

	if (check_network_msg(protocol_socket)) {	// If a message is available
	    handle_network_msg(protocol_socket, &timers);  // handle the network message
	}
	switch (check_timers(&timers)) {		// check which timer has expired
	case TIMER_BROADCAST:				// On Broadcast timer
	    broadcast_network(protocol_socket);		// send out broadcast message to contact other nodes
	    add_timer(&timers, TIMER_BROADCAST, 20);	// and set to broadcast again in y seconds
	    break;

	case TIMER_PING:
	    if (check_live_nodes(protocol_socket)) {	// On Ping check the network
		add_timer(&timers, TIMER_REPLY, 2);	// Expire replies if not received within x secoonds
		add_timer(&timers, TIMER_PING, 20);	// and set to Ping again in y seconds
	    }
	    break;

	case TIMER_REPLY:
	    expire_live_nodes();			//  Expire other nodes where reply has timed out
	    break;

	default:
	    break;
	}
	display_live_network();
//	display_timers(&timers);
    }
    return 0;
}




