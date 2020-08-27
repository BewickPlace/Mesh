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

#define NO_TIMERS 	11				// Number of Timers
#define TIMER_NONE 	-1				// Invalid Timer
#define TIMER_BROADCAST 0				// Broadcast Refresh Timer
#define TIMER_PING 	1				// Ping timer
#define TIMER_REPLY 	2				// Reply timeout timer
#define TIMER_STATS	3				// Network Statistics timer
#define TIMER_PAYLOAD_ACK 4				// Payload ACK timer
#define TIMER_APPLICATION 5				// Application timeout timer
#define TIMER_CONTROL	6				// Boost timer
#define TIMER_SETPOINT 	7				// Boost timer
#define TIMER_BOOST 	8				// Boost timer
#define TIMER_DISPLAY 	9				// Display screen timout
#define TIMER_LOGGING 	10				// Logging timeout

#define AT_STATS	(timeto1hour())			// at 0:00
#define AT_CONTROL	(timeto15min())			// at 0:00
#define AT_PING		(timetosec(20)+4)		// at 0:04, 0:24, 0:44
#define AT_SETPOINT	(timetosec(20)+8)		// at 0:08, 0:28, 0:48
#define AT_BROADCAST	(timeto1min()+52)		// at 0:52
#define AT_PROXIMITY	(32)				// at 0:32

#define AT_REPLY	3				// Timeout for PING and REPING
#define AT_PAYLOAD	3				// Timeout for initial Payload ACK
#define AT_PAYLOAD_ACK	3				// Timeout for su8bseqent ACKs

struct timer_list {					// Timer datastructure
		struct timeval timers[NO_TIMERS];	// Array of individual timers
		struct timeval wait_time;		// wait timer
	};

void    initialise_timers();

void	add_timer(int timer, int delay);

void	cancel_timer(int timer);

struct timeval *next_timers();

void	adjust_timers();

int	check_timers();

void	display_timers();

int	dayoftheweek();

int	timeto1hour();
int	timeto15min();
int	timeto5min();
int	timeto1min();
int	timetosec(int n);
