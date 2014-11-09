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

#define NO_TIMERS 2					// Number of Timers
#define TIMER_NONE -1					// Invalid Timer
#define TIMER_REFRESH 0					// Refresh Timers
#define TIMER_REPLY 1					// Reply timeout timer

struct timer_list {					// Timer datastructure
		struct timeval timers[NO_TIMERS];	// Array of individual timers
		struct timeval last_time;		// Base time when timers are valid
		struct timeval wait_time;		// wait timer
	};

void    initialise_timers(struct timer_list *timers);

void	add_timer(struct timer_list *timers, int timer, int delay);

void	cancel_timer(struct timer_list *timers, int timer);

struct timeval *next_timers(struct timer_list *timers);

int	check_timers(struct timer_list *timers);

void	display_timers(struct timer_list *timers);


