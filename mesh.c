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

#include <unistd.h>
//#include <signal.h>
//#include <assert.h>
#include <fcntl.h>
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
#include "application.h"

#define	OPMODE_MASTER	0				// Node operating modes
#define	OPMODE_SLAVE	1				//
static int	operating_mode = OPMODE_MASTER;		// Default operating mode (changed by command line)
int		packet_rate = 0;			// Number of packets sent

struct app {
    char	*logfile;
    } app;

void usage(char *progname) {
    printf("Usage: %s [options...]\n", progname);

    printf("\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");

    printf("\n");
    printf("Options:\n");
    printf("    -h, --help          show this help\n");
    printf("    -m, --master        Master operating mode\n");
    printf("    -s, --slave         Slave operating mode\n");
    printf("    -p, --packet=PACKET Number of packets sent per burst\n");

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
        {"packet",  required_argument,  NULL, 'p'},

        {"log",     required_argument,  NULL, 'l'},
        {"error",   required_argument,  NULL, 'e'},
        {NULL, 0, NULL, 0}
    };
    int opt;

    while ((opt = getopt_long(argc, argv,
                              "+hmsvp:l:e:",
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
            case 'p':
		packet_rate = atoi(optarg);			// Set  packets per burst
                break;
            case 'v':
                debuglev++;
                break;

            case 'l':
                app.logfile = optarg;
                break;
            case 'e':
//                config.errfile = optarg;
                break;
        }
    }
    return optind;
}
//
//      Open Correct Logfile
//

void    open_logfile() {
    if (app.logfile) {                                  // Logfile is specified on command line
        int log_fd = open(app.logfile,                  // Open appropriately
                O_WRONLY | O_CREAT | O_APPEND,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                                                        // Warn and continue if can't open
        ERRORCHECK( log_fd < 0, "Could not open logfile", EndError);

        dup2(log_fd, STDERR_FILENO);
        setvbuf (stderr, NULL, _IOLBF, BUFSIZ);
    }
ENDERROR;
}

//
//
//	Main procedure
//
//
int main(int argc, char **argv) {
    int shutdown = 0;					// Shutdown flag
    struct timer_list timers;				// Timers
    int payload_len;					// length of payload returned
    struct payload_pkt app_data;			// App payload data
    char node_name[HOSTNAME_LEN];			// Node name
    parse_options(argc, argv);				// Parse command line parameters
    open_logfile();
    debug(DEBUG_ESSENTIAL, "Mesh starting in mode: %d\n", operating_mode);

    initialise_network(sizeof(struct payload_pkt),notify_link_up, notify_link_down);	// Initialise the network details with callbacks
    initialise_timers(&timers);				// and set all timers

    switch (operating_mode) {
    case OPMODE_MASTER:					// Only Master nodes are responsible for broadcasting
	add_timer(TIMER_BROADCAST, 5);			// Set to refresh network in y seconds
	break;

    case OPMODE_SLAVE:
//	add_timer(TIMER_APPLICATION, 15);		// Set to refresh network in y seconds
//	now done when link comes up
	break;
    }

    if (debuglev == DEBUG_ESSENTIAL) {
	add_timer(TIMER_STATS, timeto1hour());		// Report Network Efficiency stats hourly
    } else {
	add_timer(TIMER_STATS, timeto1min());		// Report Network Efficiency stats regularly
    }

    while (!shutdown) {					// While NOT shutdown
	wait_on_network_timers(&timers); 		// Wait for message or timer expiory

	if (check_network_msg()) {			// If a message is available
	    handle_network_msg(&node_name[0], (char *)&app_data, &payload_len);	// handle the network message
	    handle_app_msg(&app_data, payload_len);			// handle application specific messages
	}
	switch (check_timers(&timers)) {		// check which timer has expired
	case TIMER_BROADCAST:				// On Broadcast timer
	    broadcast_network();			// send out broadcast message to contact other nodes
	    add_timer(TIMER_BROADCAST, 20);		// and set to broadcast again in y seconds
	    break;

	case TIMER_PING:
	    if (check_live_nodes()) {			// On Ping check the network
		add_timer(TIMER_REPLY, 2);		// Expire replies if not received within x secoonds
		add_timer(TIMER_PING, 20);		// and set to Ping again in y seconds
	    }
	    break;

	case TIMER_REPLY:
	    expire_live_nodes();			//  Expire other nodes where reply has timed out
	    break;

	case TIMER_STATS:
	    report_network_stats();			// Report network efficiency stats hourly
	    if (debuglev == DEBUG_ESSENTIAL) {
		add_timer(TIMER_STATS, timeto1hour());		// Report Network Efficiency stats hourly
	    } else {
		add_timer(TIMER_STATS, timeto1min());		// Report Network Efficiency stats regularly
	    }
	    break;

	case TIMER_PAYLOAD_ACK:
	    timeout_payload();
	    break;

	case TIMER_APPLICATION:
	    handle_app_timer();				// Handle the timer for the App
	    break;

	default:
	    break;
	}
	DEBUG_FUNCTION( DEBUG_DETAIL, display_live_network());
	DEBUG_FUNCTION( DEBUG_DETAIL, display_timers(&timers));
    }
    debug(DEBUG_ESSENTIAL, "Mesh node shut down\n");
    return 0;
}




