/*
Copyright (c) 2007-2010 by Juliusz Chroboczek

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

//
//
//	Main procedure
//
//
int main(int argc, char **argv) {
    int shutdown = 0;					// Shutdown flag
    int protocol_socket;				// Network protocol socket
    struct timer_list timers;				// Timers

    protocol_socket =  initialise_network();		// Initialise the network details
    initialise_timers(&timers);				// and set all timers

    add_timer(&timers, TIMER_REFRESH, 20);		// Set to refresh network very 10 seconds

    while (!shutdown) {					// While NOT shutdown
	printf("====================\n");
	wait_on_network_timers(protocol_socket, &timers); // Wait for message or timer expiory

	if (check_network_msg(protocol_socket)) {	// If a message is available
	    handle_network_msg(protocol_socket); 	// handle the network message
	}
	switch (check_timers(&timers)) {		// check which timer has expired
	case TIMER_REFRESH:
	    if (check_live_nodes(protocol_socket)) {	// On refresh check the network
		 add_timer(&timers, TIMER_REPLY, 2);	// Expire replies if not received within x secoonds
	    }
	    add_timer(&timers, TIMER_REFRESH, 20);	// and set to refresh again in y seconds
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




