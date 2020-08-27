/*
Copyright (c) 2014-  by John Chandler

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
#include <wiringPi.h>

#include "errorcheck.h"
#include "timers.h"
#include "networks.h"
#include "application.h"

void	notify_link_up() {
    debug(DEBUG_ESSENTIAL, "UP\n");

    add_timer(TIMER_APPLICATION, 1);
    }

void	notify_link_down() {
    debug(DEBUG_ESSENTIAL, "DOWN\n");

    cancel_timer(TIMER_APPLICATION);
    }

void	handle_app_msg(struct payload_pkt *payload, int payload_len) {

    if( payload_len == 0) { return; }		// skip out if nothing to do

    debug(DEBUG_DETAIL, "Payload Received of type %d %s, len %d\n", payload->type, payload->data, payload_len);
}

void	handle_app_timer(int sock) {
    struct payload_pkt app_data = {PAYLOAD_TYPE,"TEST MSG...\0" };
    int node = 0;
    int i;

    debug(DEBUG_INFO, "Handle App timeout >>> send burst of messages\n");

    for (i=0; i< 20; i++) {			// Send a burst of test messages
	send_to_node(node, (char *) &app_data, sizeof(app_data));
	delay(50);
    }

    add_timer(TIMER_APPLICATION, 5);		// try again in n sec
    }
