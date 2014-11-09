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

#define	NO_NETWORKS 10

#define NET_STATE_UNKNOWN 0
#define NET_STATE_DOWN 	1
#define NET_STATE_UP 	2

#define MSG_STATE_UNKNOWN 0
#define MSG_STATE_SENT 	1
#define MSG_STATE_RECEIVED 2
#define MSG_STATE_FAILED 3
#define MSG_STATE_OK 	4

int	initialise_network();

void	wait_on_network_timers(int sock, struct timer_list *timers);

int	check_live_nodes(int sock);

void	expire_live_nodes();

void	display_live_network();

int	check_network_msg(int sock);

void	handle_network_msg(int sock, struct timer_list *timers);
