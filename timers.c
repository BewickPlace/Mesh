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

static struct timer_list timers;			// Hidden timer list

#define	MAXIMUM_WAIT_TIME 100

	//
	//	Initialise the timer data structure
	//
void    initialise_timers() {

	memset(&timers, 0, sizeof(timers));		// Set all timers to zero (no timer running)
	timers.wait_time.tv_sec = MAXIMUM_WAIT_TIME;	// & default wait timer
	}

	//
	//	Add the chosen timer to the list
	//
void	 add_timer(int timer, int delay) {
	struct timeval now;

	gettimeofday(&now, NULL);
	timers.timers[timer].tv_sec = now.tv_sec + (time_t)delay;	// Set chosen timer
	}

	//
	//	Cancel the chosen timer to the list
	//
void	 cancel_timer(int timer) {

	timers.timers[timer].tv_sec = 0;		// Set chosen timer to zero
	}


static struct timeval 	next;
static time_t		timeout;
	//
	//	Return the address of the next timer due
	//
struct timeval *next_timers() {
	int i;
	time_t lowest_time = MAXIMUM_WAIT_TIME;
	struct timeval now;

	gettimeofday(&now, NULL);

	for (i=0; i < NO_TIMERS; i++) {			// Check all of the timers looking for lowest time value
	    if ((timers.timers[i].tv_sec != 0) &&	// zero is not included as that is expired
	        ((timers.timers[i].tv_sec - now.tv_sec) < lowest_time)) {
		lowest_time = timers.timers[i].tv_sec - now.tv_sec; // Record lowest time
	    }
	}
	next = now;
	timeout = lowest_time;
	timers.wait_time.tv_sec = lowest_time;  	// Copy to timer slot
	timers.wait_time.tv_usec = 0;		  	// Copy to timer slot
	return(&timers.wait_time);			// Return address of lowest timer - NULL if none valid
	}

	//
	//	Check to see if time adjusted
	//
void	adjust_timers() {
	struct timeval now;
	time_t diff;
        int i;

	gettimeofday(&now, NULL);			// Check if gap bewteen calls is too big
	diff = now.tv_sec - next.tv_sec - (timeout - timers.wait_time.tv_sec + 1); // allowing a flex of 1
	if (diff > 0 ) {
	    warn("Time adjusted by %ld seconds", diff);

	    for (i=0; i < NO_TIMERS; i++) {		// Update all active timers
	        if (timers.timers[i].tv_sec != 0) {
		    timers.timers[i].tv_sec = timers.timers[i].tv_sec + diff;	// by the difference identified
	    	}
	    }
	}
}

	//
	//	Check Timers for expired timer - decrement others
	//
int	check_timers() {
	int i;
	struct timeval now;
	int expired_timer = TIMER_NONE;

	gettimeofday(&now, NULL);			// Get current time

	for (i=0; i < NO_TIMERS; i++) {			// Check all valid timers
	    if ((timers.timers[i].tv_sec != 0) &&
	        (now.tv_sec >= timers.timers[i].tv_sec)) {  // If timer is due to expire
		timers.timers[i].tv_sec = 0;		// Expire timer
		expired_timer = i;			// and note index of timer
		break;
	    }
	}
	return(expired_timer);
}

	//
	//	Display the timers
	//
void	display_timers() {
	int i;
	struct timeval now;

	gettimeofday(&now, NULL);

	for (i=0; i < NO_TIMERS; i++) {
	    if (timers.timers[i].tv_sec != 0) {
		debug(DEBUG_DETAIL, "Timer: %d - value %ld\n", i, (long)(timers.timers[i].tv_sec - now.tv_sec));
	    } else {
		debug(DEBUG_DETAIL, "Timer: %d - value %ld\n", i, 0L);
	    }
	}
}

//
//	get the day of the week (0..6) aligned Mon-Sunt
//	note this matches the profiles index
//

int	dayoftheweek() {
    time_t	seconds;
    struct tm	*info;

    seconds = time(NULL);		// get the time
    info = localtime(&seconds);		// convert into strctured time
    return((info->tm_wday+6)%7);	// return the day of the week, aligned Mon to Sun
}

//
//	get the time (in secs) to next X minute bouldry
//
int timetoXmin(int X) {
    time_t	seconds = time(NULL);;  // get the time

    seconds = seconds % (X * 60);	// get the remauinder of X minutes
    seconds = (X * 60) - seconds;		// and establish the to go until the next boundry
    return((int)seconds);		// return this figure
}

int	timeto1hour() { return(timetoXmin(60)); }
int	timeto15min() { return(timetoXmin(15)); }
int	timeto5min()  { return(timetoXmin(5)); }
int	timeto1min()  { return(timetoXmin(1)); }
int	timetosec(int n) {
    int x = timeto1min() % n;
    x = (x==0 ? n : x);
    return (x);
}
