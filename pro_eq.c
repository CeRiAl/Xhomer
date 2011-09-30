/* pro_eq.c: Event queue

   Copyright (c) 1997-2003, Tarik Isani (xhomer@isani.org)

   This file is part of Xhomer.

   Xhomer is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 
   as published by the Free Software Foundation.

   Xhomer is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Xhomer; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifdef PRO
#include "pdp11_defs.h"

unsigned char PRO_EQ[PRO_EQ_SIZE];

int	pro_eq_ptr;	/* pointer to current location in event queue */

void (*pro_event[PRO_EQ_NUM_EVENTS+1])(void) =
	{
	  NULL,
	  &pro_clk_eq,
	  &pro_clk_uf_eq,
	  &pro_vid_eq,
	  &pro_vid_cmd_eq,
	  &pro_2661kb_eq,
	  &pro_2661kb_poll_eq,
	  &pro_2661ptr_eq,
	  &pro_2661ptr_poll_eq,
	  &pro_com_eq,
	  &pro_com_poll_eq,
	  &pro_rd_eq
	};


/* Schedule event */

void pro_eq_sched (int eventnum, int interval)
{
int	i, j;

	/* Check for collision with other pending events */

	/* XXX This assumes at most one event of each type
	   can exist in the queue at a time */

	for(i=0; i<PRO_EQ_NUM_EVENTS; i++)
	{
	  j = (pro_eq_ptr + interval + i) & PRO_EQ_MASK;

	  /* Check if queue entry is empty */

	  if (PRO_EQ[j] == 0)
	  {
	    PRO_EQ[j] = eventnum;
	    break;
	  }
	}
}


/* Reset */

void pro_eq_reset ()
{
	/* Initialize event queue */

	memset(&PRO_EQ, 0, sizeof(PRO_EQ));

	pro_eq_ptr = 0;
}

#endif
