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

#define NO_TIMERS 	10				// Number of Timers
#define TIMER_NONE 	-1				// Invalid Timer
#define TIMER_BROADCAST 0				// Broadcast Refresh Timer
#define TIMER_PING 	1				// Ping timer
#define TIMER_REPLY 	2				// Reply timeout timer
#define TIMER_STATS	3				// Network Statistics timer
#define TIMER_APPLICATION 4				// Application timeout timer
#define TIMER_CONTROL	5				// Boost timer
#define TIMER_SETPOINT 	6				// Boost timer
#define TIMER_BOOST 	7				// Boost timer
#define TIMER_DISPLAY 	8				// Display screen timout
#define TIMER_LOGGING 	9				// Logging timeout

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
