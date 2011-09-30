/* pro_2661kb.c: serial port controller (used by kb & ptr)

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


/* TBD:
		-implement reading of line status
			break
			parity error
		-update line parameters only if something changed
		-overrun is not implemented.  Is it needed for loopback?
		-do global mask for odd byte writes
*/

#ifdef PRO
#include "pdp11_defs.h"

/* XXX make generic */

LOCAL int	pro_2661kb_data;
LOCAL int	pro_2661kb_wdata;
LOCAL int	pro_2661kb_stat;
LOCAL int	pro_2661kb_mr1;
LOCAL int	pro_2661kb_mr2;
LOCAL int	pro_2661kb_cmd;

LOCAL int	pro_2661kb_mode_ptr;	/* Indicates which mode register is to be accessed */

struct serctrl	pro_2661kb_ctrl;	/* Stores line control parameters */


LOCAL void pro_2661kb_ctrl_get ()
{
	pro_kb->ctrl_get(PRO_SER_KB, &pro_2661kb_ctrl);

	pro_2661kb_stat = (pro_2661kb_stat & ~PRO_2661_DSR);

	if (pro_2661kb_ctrl.dsr == 1)
	  pro_2661kb_stat |= PRO_2661_DSR;
}


LOCAL void pro_2661kb_ctrl_put ()
{
	pro_2661kb_ctrl.cs = (pro_2661kb_mr1 & PRO_2661_CL) >> 2;
	pro_2661kb_ctrl.stop = (pro_2661kb_mr1 & PRO_2661_SBL) >> 6;
	pro_2661kb_ctrl.parity = (pro_2661kb_mr1 & PRO_2661_PT) >> 5;
	pro_2661kb_ctrl.penable = (pro_2661kb_mr1 & PRO_2661_PC) >> 4;
	pro_2661kb_ctrl.ibaud = pro_2661kb_mr2 & PRO_2661_BAUD;
	pro_2661kb_ctrl.obaud = pro_2661kb_mr2 & PRO_2661_BAUD;
	pro_2661kb_ctrl.dtr = (pro_2661kb_cmd & PRO_2661_DTR) >> 1;
	pro_2661kb_ctrl.obrk = (pro_2661kb_cmd & PRO_2661_FB) >> 3;

	/* Hardwire rts */

	pro_2661kb_ctrl.rts = 1;

	/* XXX Update serial line parameters only if they changed */

	pro_kb->ctrl_put(PRO_SER_KB, &pro_2661kb_ctrl);
}


/* Polling event queue scheduler */

LOCAL void pro_2661kb_poll_sched ()
{
	pro_eq_sched(PRO_EVENT_KB_POLL, PRO_EQ_KB_POLL);
}


/* Polling event queue handler */

void pro_2661kb_poll_eq ()
{
int	schar;

	/* Only poll if receiver done is cleared, to avoid overrun */

	/* Check if receiver is enabled */

	if (((pro_2661kb_cmd & PRO_2661_OM) == PRO_2661_NORMAL)
	   && ((pro_2661kb_cmd & PRO_2661_RXEN) != 0)
	   && ((pro_2661kb_stat & PRO_2661_RD) == 0))
	{
	  schar = pro_kb->get(PRO_SER_KB);

	  if (schar != PRO_NOCHAR)
	  {
	    /* Copy received character to input buffer */

	    pro_2661kb_data = schar;

	    /* Set receiver done bit */

	    pro_2661kb_stat = pro_2661kb_stat | PRO_2661_RD;

	    /* Set receiver interrupt event */

	    pro_int_set(PRO_INT_KB_RCV);
	  }
	}

	/* Schedule next polling event */

	pro_2661kb_poll_sched();
}


/* Write data event handler */

void pro_2661kb_eq ()
{
	/* Queue another write data event if in NORMAL mode
	   and character send failed.  Otherwise, complete write
	   data event */

	if ((pro_2661kb_cmd & PRO_2661_OM) == PRO_2661_NORMAL)
	{
	  if (pro_kb->put(PRO_SER_KB, pro_2661kb_wdata) == PRO_FAIL)
	  {
	    pro_eq_sched(PRO_EVENT_KB, PRO_EQ_KB_RETRY);
	    return;
	  }
	}

	/* Set transmitter ready bit */

	pro_2661kb_stat = pro_2661kb_stat | PRO_2661_TR;
	
	/* Set transmitter intterrupt */

	pro_int_set(PRO_INT_KB_XMIT);

	/* Set receive int+rd if local loopback is enabled */

	/* XXX is a delay between xmit and rcv interrupts needed? */

	/* XXX does this cause an int+rd if RXEN is disabled? */

	/* XXX various bits needed for loopback are not checked */

	if ((pro_2661kb_cmd & PRO_2661_OM) == PRO_2661_LOOPBACK)
	{
	  /* Loop write data back */

	  pro_2661kb_data = pro_2661kb_wdata;

	  /* Set receiver done bit */

	  pro_2661kb_stat = pro_2661kb_stat | PRO_2661_RD;

	  /* Set receiver interrupt */

	  pro_int_set(PRO_INT_KB_RCV);
	}
}


/* 2661 serial controller registers */

int pro_2661kb_rd (int pa)
{
int	data;

	switch (pa & 06)
	{
	  case 00:
	    data = pro_2661kb_data;

	    pro_2661kb_stat = pro_2661kb_stat & ~PRO_2661_RD; /* clear rec. done bit */

	    break;

	  case 02:
	    /* Get serial line status first */

	    pro_2661kb_ctrl_get();
	    data = pro_2661kb_stat;
	    break;

	  case 04:
	    if (pro_2661kb_mode_ptr == 0)
	    {
	      data = pro_2661kb_mr1;
	      pro_2661kb_mode_ptr = 1;
	    }
	    else
	    {
	      data = pro_2661kb_mr2;
	      pro_2661kb_mode_ptr = 0;
	    }
	    break;

	  case 06:
	    data = pro_2661kb_cmd;

	    if (pro_2661kb_mode_ptr == 0)
	      pro_2661kb_mode_ptr = 1;
	    else
	      pro_2661kb_mode_ptr = 0;

	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_2661kb_wr (int data, int pa, int access)
{
	switch (pa & 06)
	{
	  case 00:
	    if ((pro_2661kb_cmd & PRO_2661_TXEN) != 0)
	    {
	      /* Store write data value for use in event handler */

	      WRITE_WB(pro_2661kb_wdata, PRO_2661_DATA_W, access);

	      /* Clear transmitter ready bit */

	      pro_2661kb_stat = pro_2661kb_stat & ~PRO_2661_TR;

	      /* Queue write data event */

	      pro_eq_sched(PRO_EVENT_KB, PRO_EQ_KB);
	    }

	    break;

	  case 04:
	    if (pro_2661kb_mode_ptr == 0)
	    {
	      WRITE_WB(pro_2661kb_mr1, PRO_2661_MR1_W, access);
	      pro_2661kb_mode_ptr = 1;
	    }
	    else
	    {
	      WRITE_WB(pro_2661kb_mr2, PRO_2661_MR2_W, access);
	      pro_2661kb_mode_ptr = 0;
	    }

	    /* Update line parameters */

	    pro_2661kb_ctrl_put();
	    break;

	  case 06:
	    /* XXX Reset error unimplemented */

	    WRITE_WB(pro_2661kb_cmd, PRO_2661_CMD_W, access);

	    /* Clear receiver done if receiver is disabled */

	    if ((pro_2661kb_cmd & PRO_2661_RXEN) == 0)
	      pro_2661kb_stat = pro_2661kb_stat & ~PRO_2661_RD;

	    /* Clear transmitter ready if auto-echo or remote loopback modes */

	    if (((pro_2661kb_cmd & PRO_2661_OM0) == 1)
	       || ((pro_2661kb_cmd & PRO_2661_TXEN) == 0))
	      pro_2661kb_stat = pro_2661kb_stat & ~PRO_2661_TR;
	    else
	      pro_2661kb_stat = pro_2661kb_stat | PRO_2661_TR;

	    /* Update line parameters */

	    pro_2661kb_ctrl_put();
	    break;

	  default:
	    break;
	}
}


void pro_2661kb_reset ()
{
	pro_2661kb_data = 0000;
	pro_2661kb_stat = 0100 | PRO_2661_TR;	/* transmit ready, cleared for auto-echo or rem. loopback */
	pro_2661kb_mr1 = 0000;
	pro_2661kb_mr2 = 0000;
	pro_2661kb_cmd = 0000;

	pro_2661kb_mode_ptr = 0;

	/* Initialize serial line parameters */

	memset(&pro_2661kb_ctrl, 0, sizeof(pro_2661kb_ctrl));

	/* Schedule 2661 polling event */

	pro_2661kb_poll_sched();
}

void pro_2661kb_open ()
{
	/* Open serial port */

	pro_kb->reset(PRO_SER_KB, pro_kb_port);

	/* Set and get serial line parameters */

	pro_2661kb_ctrl_put();
	pro_2661kb_ctrl_get();
}
#endif
