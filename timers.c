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
//#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>
#include <time.h>

//#include <unistd.h>
//#include <signal.h>
//#include <assert.h>
//#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/uio.h>
//#include <netinet/in.h>
//#include <netinet/ip.h>
//#include <arpa/inet.h>
//#include <net/if.h>

#include "errorcheck.h"
#include "timers.h"

	//
	//	Initialise the timer data structure
	//
void    initialise_timers(struct timer_list *timers) {

	memset(timers, 0, sizeof(*timers));		// Set all timers to zero (no timer running)
        gettimeofday(&(timers->last_time), NULL);	// Record the current time as timer base
	}

	//
	//	Add the chosen timer to the list
	//
void	 add_timer(struct timer_list *timers, int timer, int delay) {

	timers->timers[timer].tv_sec = (time_t)delay;	// Set chosen timer
	}

	//
	//	Cancel the chosen timer to the list
	//
void	 cancel_timer(struct timer_list *timers, int timer) {

	timers->timers[timer].tv_sec = 0;		// Set chosen timer to zero
	}

	//
	//	Return the address of the next timer due
	//
struct timeval *next_timers(struct timer_list *timers) {
	int i;
	time_t lowest_time = 999;

	for (i=0; i < NO_TIMERS; i++) {			// Check all of the timers looking for lowest time value
	    if ((timers->timers[i].tv_sec != 0) &&	// zero is not included as that is expired
	        (timers->timers[i].tv_sec < lowest_time)) {
	       lowest_time = timers->timers[i].tv_sec;	// Record lowest time
	       timers->wait_time.tv_sec = lowest_time;  // Copy to timer slot
	    }
	}
	return(&timers->wait_time);			// Return address of lowest timer - NULL if none valid
	}

	//
	//	Check Timers for expired timer - decrement others
	//
int	check_timers(struct timer_list *timers) {
	int i;
	time_t diff;
	struct timeval now;
	int expired_timer = TIMER_NONE;

	gettimeofday(&now, NULL);			// Get current time
	diff = now.tv_sec - timers->last_time.tv_sec;	// Work out difference from last baseline

	for (i=0; i < NO_TIMERS; i++) {			// Check all valid timers
	    if (timers->timers[i].tv_sec != 0) {
		 if (diff < timers->timers[i].tv_sec) {  // If timer if not due to expire
		    timers->timers[i].tv_sec = timers->timers[i].tv_sec - diff;	// Decrement timer
		 } else {
		    timers->timers[i].tv_sec = 0;	// Expire timer
		    expired_timer = i;			// and note index of timer
	         }
	    }
	}
	timers->last_time.tv_sec = now.tv_sec;		// Record new base time
	return(expired_timer);
}

	//
	//	Display the timers
	//
void	display_timers(struct timer_list *timers) {
	int i;

	printf("Timers:  ");
	for (i=0; i < NO_TIMERS; i++) {
	    printf("%d - value %lld  ", i, (long long)timers->timers[i].tv_sec);
	}
	printf("valid at %llds\n", (long long)timers->last_time.tv_sec);
}