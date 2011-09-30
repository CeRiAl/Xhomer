/* pro_com.c: communication port

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
		-do global mask for odd byte writes
		-receive character
			check RIE
		-int on modem line change
		-finish implementing commands
		-get line controls
			break
			parity error
		-sync modes are not implemented
		-assumes 16x clock
		-transmitter character length is ignored and
                 assumed to be the same as for the receiver
		-parity bit is not present as msb data for <8-bit formats
*/

#ifdef PRO
#include "pdp11_defs.h"


LOCAL int pro_com_a_wr0;			/* control register A WR0 */
LOCAL int pro_com_a_wr1;			/* control register A WR1 */
LOCAL int pro_com_a_wr2;			/* control register A WR2 */
LOCAL int pro_com_a_wr3;			/* control register A WR3 */
LOCAL int pro_com_a_wr4;			/* control register A WR4 */
LOCAL int pro_com_a_wr5;			/* control register A WR5 */
LOCAL int pro_com_a_wr6;			/* control register A WR6 */
LOCAL int pro_com_a_wr7;			/* control register A WR7 */
LOCAL int pro_com_a_rr0;			/* status register A RR0 */
LOCAL int pro_com_a_rr1;			/* status register A RR1 */
LOCAL int pro_com_a_rr2;			/* status register A RR2 */
LOCAL int pro_com_b_wr0;			/* control register B WR0 */
LOCAL int pro_com_b_wr1;			/* control register B WR1 */
LOCAL int pro_com_b_wr2;			/* control register B WR2 */
LOCAL int pro_com_mcr0;				/* modem control register 0 */
LOCAL int pro_com_mcr1;				/* modem control register 1 */
LOCAL int pro_com_baud;				/* baud rates */
LOCAL int pro_com_wdata;			/* write data */

LOCAL int pro_com_dataq[PRO_COM_DATAQ_DEPTH+1];	/* read data queue */
LOCAL int pro_com_dataq_h;			/* head of queue */
LOCAL int pro_com_dataq_t;			/* tail of queue */

LOCAL int pro_com_intq[PRO_COM_INTQ_DEPTH+1];	/* pending interrupt queue */
LOCAL int pro_com_intq_h;			/* head of queue */
LOCAL int pro_com_intq_t;			/* tail of queue */

struct serctrl pro_com_ctrl;			/* Stores line control parameters */


LOCAL void pro_com_ctrl_get ()
{
	pro_com->ctrl_get(PRO_SER_COM, &pro_com_ctrl);

	pro_com_mcr1 &= ~(PRO_COM_DSR | PRO_COM_RI | PRO_COM_CTS | PRO_COM_CD);
	pro_com_mcr1 |= pro_com_ctrl.dsr ? PRO_COM_DSR : 0;
	pro_com_mcr1 |= pro_com_ctrl.ri ? PRO_COM_RI : 0;
	pro_com_mcr1 |= pro_com_ctrl.cts ? PRO_COM_CTS : 0;
	pro_com_mcr1 |= pro_com_ctrl.cd ? PRO_COM_CD : 0;
}


LOCAL void pro_com_ctrl_put ()
{
	pro_com_ctrl.cs = (pro_com_a_wr3 & PRO_COM_RCL1) >> 7
	                  | (pro_com_a_wr3 & PRO_COM_RCL0) >> 5;
	pro_com_ctrl.stop = (pro_com_a_wr4 & PRO_COM_SB) >> 2;
	pro_com_ctrl.parity = (pro_com_a_wr4 & PRO_COM_EO) >> 1;
	pro_com_ctrl.penable = pro_com_a_wr4 & PRO_COM_PEN;
	pro_com_ctrl.ibaud = pro_com_baud & PRO_COM_RBR;
	pro_com_ctrl.obaud = (pro_com_baud & PRO_COM_TBR) >> 4;
	pro_com_ctrl.dtr = (pro_com_mcr0 & PRO_COM_DTR) >> 4;
	pro_com_ctrl.rts = (pro_com_mcr0 & PRO_COM_RTS) >> 3;
	pro_com_ctrl.obrk = (pro_com_a_wr5 & PRO_COM_SBRK) >> 4;

	/* XXX Update serial line parameters only if they changed */

	pro_com->ctrl_put(PRO_SER_COM, &pro_com_ctrl);
}


/* Queue new received character */

LOCAL void pro_com_qdata (int data)
{
	/* Add character at tail of queue */

	pro_com_dataq[pro_com_dataq_t] = data;
	pro_com_dataq_t++;
	if (pro_com_dataq_t == (PRO_COM_DATAQ_DEPTH + 1))
	  pro_com_dataq_t = 0;

	/* Set "receive character available" bit */

	pro_com_a_rr0 |= PRO_COM_RXCA;
}


/* Queue new interrupt */

LOCAL void pro_com_qint (int intnum)
{
	/* Add interrupt at tail of queue */

	pro_com_intq[pro_com_intq_t] = intnum;
	pro_com_intq_t++;
	if (pro_com_intq_t == (PRO_COM_INTQ_DEPTH + 1))
	  pro_com_intq_t = 0;
}


/* Read poll event handler */

void pro_com_poll_eq ()
{
int	schar;

	/* Only poll if RXCA is cleared, the receiver is enabled,
	   and the communication port is not in maintenance mode */

	if (((pro_com_mcr0 & PRO_COM_MM) == 0)
	   && ((pro_com_a_wr3 & PRO_COM_RXEN) != 0)
	   && ((pro_com_a_rr0 & PRO_COM_RXCA) == 0))
	{
	  schar = pro_com->get(PRO_SER_COM);

	  if (schar != PRO_NOCHAR)
	  {
	    pro_com_qdata(schar);

	    /* Set receiver interrupt */
	    /* XXX check if enabled */

	    pro_com_qint(PRO_COM_IR_RCV);
	    pro_int_set(PRO_INT_COM);
	  }
	}

	/* Schedule next polling event */

	pro_eq_sched(PRO_EVENT_COM_POLL, PRO_EQ_COM_POLL);
}


/* Write data event handler */

void pro_com_eq ()
{
	/* Queue another write data event if not in maintenance mode
	   and character send failed.  Otherwise, complete write
	   data event */

	if ((pro_com_mcr0 & PRO_COM_MM) == 0)
	{
	  if (pro_com->put(PRO_SER_COM, pro_com_wdata) == PRO_FAIL)
	  {
	    pro_eq_sched(PRO_EVENT_COM, PRO_EQ_COM_RETRY);
	    return;
	  }
	}

	/* Set "transmit buffer empty" and "all sent bits" */

	pro_com_a_rr0 |= PRO_COM_TBMT;
	pro_com_a_rr1 |= PRO_COM_AS;

	/* Check if transmitter is enabled */

	if ((pro_com_a_wr5 & PRO_COM_TXEN) != 0)
	{
	  /* Set transmitter interrupt, if enabled */

	  if ((pro_com_a_wr1 & PRO_COM_TIE) != 0)
	  {
	    pro_com_qint(PRO_COM_IR_XMIT);
	    pro_int_set(PRO_INT_COM);
	  }

	  /* Check if in maintenance mode */

	  if ((pro_com_mcr0 & PRO_COM_MM) != 0)
	  {
	    /* Loop data back to received character queue,
	       if receiver enabled */

	    if ((pro_com_a_wr3 & PRO_COM_RXEN) != 0)
	    {
	      pro_com_qdata(pro_com_wdata);

	      /* Set receiver interrupt */
	      /* XXX check if enabled */

	      pro_com_qint(PRO_COM_IR_RCV);
	      pro_int_set(PRO_INT_COM);
	    }
	  }
	}
}


/* Communication port registers */

int pro_com_rd (int pa)
{
int	data;

	switch (pa & 017777776)
	{
	  case 017773300:
	    data = pro_com_dataq[pro_com_dataq_h];

	    /* Remove read character from data queue */

	    if (pro_com_dataq_h != pro_com_dataq_t)
	    {
	      pro_com_dataq_h++;

	      if (pro_com_dataq_h == (PRO_COM_DATAQ_DEPTH + 1))
	        pro_com_dataq_h = 0;
	    }

	    /* Clear "receive character available" bit if
	       receiver buffer is empty */

	    if (pro_com_dataq_h == pro_com_dataq_t)
	      pro_com_a_rr0 &= ~PRO_COM_RXCA;

	    break;

	  case 017773302:
	    switch (pro_com_a_wr0 & PRO_COM_RP)
	    {
	      case 00:
	        data = pro_com_a_rr0;
	        break;

	      case 01:
	        data = pro_com_a_rr1;
	        break;

	      case 02:
	        data = pro_com_a_rr2;
	        break;

	      default:
	        data = 0;
	        break;
	    }

	    /* Reset register pointer to wr0 */

	    pro_com_a_wr0 = pro_com_a_wr0 & ~PRO_COM_RP;

	    break;

	  case 017773306:
	    if ((pro_com_b_wr0 & PRO_COM_RP) == 02)
	    {
	      /* Report pending interrupt if queue not empty */

	      if (pro_com_intq_h != pro_com_intq_t)
	      {
	        data = pro_com_intq[pro_com_intq_h];

	        /* Set interrupt pending bit */

	        pro_com_a_rr0 |= PRO_COM_INTP;
	      }
	      else
	        data = PRO_COM_IR_NONE;

	      /* Or in vector from wr2 */

	      data |= (pro_com_b_wr2 & PRO_COM_VEC);
	    }
	      else
	        data = 0;

	    /* Reset register pointer to wr0 */

	    pro_com_b_wr0 = pro_com_b_wr0 & ~PRO_COM_RP;

	    break;

	  case 017773310:
	    data = pro_com_mcr0;
	    break;

	  case 017773312:
	    /* Get modem control lines */

	    pro_com_ctrl_get();

	    data = pro_com_mcr1;
	    break;

	  /* Write-only */

	  case 017773304:
	  case 017773314:

	    data = 0;
	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_com_wr (int data, int pa, int access)
{
	switch (pa & 017777776)
	{
	  case 017773300:
	    /* Check if transmitter enabled */

	    if ((pro_com_a_wr5 & PRO_COM_TXEN) != 0)
	    {
	      /* Store write data for use in event handler */

	      WRITE_WB(pro_com_wdata, PRO_COM_W, access);

	      /* Clear "transmit buffer empty" and "all sent bits" */

	      pro_com_a_rr0 &= ~PRO_COM_TBMT;
	      pro_com_a_rr1 &= ~PRO_COM_AS;
	    
	      /* Queue write data event */

	      pro_eq_sched(PRO_EVENT_COM, PRO_EQ_COM);
	    }

	    break;

	  case 017773302:
	    if ((pro_com_a_wr0 & PRO_COM_RP) == 00)
	    {
	      WRITE_WB(pro_com_a_wr0, PRO_COM_W, access);

	      /* Decode commands */
	      /* XXX Unfinished */

	      switch (pro_com_a_wr0 & PRO_COM_CMD)
	      {
	        case PRO_COM_EOI: /* end of interrupt */

	          /* Clear interrupt at head of queue */

	          if (pro_com_intq_h != pro_com_intq_t)
	          {
	            pro_com_intq_h++;

	            if (pro_com_intq_h == (PRO_COM_INTQ_DEPTH + 1))
	              pro_com_intq_h = 0;

	            /* Set new interrupt if queue not empty,
	               otherwise, clear interrupt pending bit */

	            if (pro_com_intq_h != pro_com_intq_t)
	              pro_int_set(PRO_INT_COM);
	            else
	              pro_com_a_rr0 &= ~PRO_COM_INTP;
	          }
	          break;

	        default:
	          break;
	      }
	    }
	    else
	    {
	      switch (pro_com_a_wr0 & PRO_COM_RP)
	      {
	        case 01:
	          WRITE_WB(pro_com_a_wr1, PRO_COM_W, access);
	          break;

	        case 02:
	          WRITE_WB(pro_com_a_wr2, PRO_COM_W, access);
	          break;

	        case 03:
	          WRITE_WB(pro_com_a_wr3, PRO_COM_W, access);

	          /* Update line parameters */

	          pro_com_ctrl_put();
	          break;

	        case 04:
	          WRITE_WB(pro_com_a_wr4, PRO_COM_W, access);

	          /* Update line parameters */

	          pro_com_ctrl_put();
	          break;

	        case 05:
	          WRITE_WB(pro_com_a_wr5, PRO_COM_W, access);

	          /* Update line parameters */

	          pro_com_ctrl_put();
	          break;

	        case 06:
	          WRITE_WB(pro_com_a_wr6, PRO_COM_W, access);
	          break;

	        case 07:
	          WRITE_WB(pro_com_a_wr7, PRO_COM_W, access);
	          break;

	        default:
	          break;
	      }

	      /* Reset register pointer to wr0 */

	      pro_com_a_wr0 = pro_com_a_wr0 & ~PRO_COM_RP;
	    }

	    break;

	  case 017773306:
	    if ((pro_com_b_wr0 & PRO_COM_RP) == 00)
	      WRITE_WB(pro_com_b_wr0, PRO_COM_W, access);
	    else
	    {
	      switch (pro_com_b_wr0 & PRO_COM_RP)
	      {
	        case 01:
	          WRITE_WB(pro_com_b_wr1, PRO_COM_W, access);
	          break;

	        case 02:
	          WRITE_WB(pro_com_b_wr2, PRO_COM_W, access);
	          break;

	        default:
	          break;
	      }

	      /* Reset register pointer to wr0 */

	      pro_com_b_wr0 = pro_com_b_wr0 & ~PRO_COM_RP;
	    }

	    break;

	  case 017773310:
	    WRITE_WB(pro_com_mcr0, PRO_COM_W, access);
	    break;

	  case 017773314:
	    WRITE_WB(pro_com_baud, PRO_COM_W, access);

	    /* Update line parameters */

	    pro_com_ctrl_put();
	    break;

	  /* Read-only */

	  case 017773304:
	  case 017773312:

	    break;

	  default:
	    break;
	}
}

void pro_com_reset ()
{
int	i;


	/* Initialize serial line parameters */

	memset(&pro_com_ctrl, 0, sizeof(pro_com_ctrl));

	pro_com_dataq_h = 0;	/* clear data queue */
	pro_com_dataq_t = 0;

	/* Not necessary, but clean */

	for(i=0; i<PRO_COM_DATAQ_DEPTH; i++)
	  pro_com_dataq[i] = 0;

	pro_com_intq_h = 0;	/* clear interrupt queue */
	pro_com_intq_t = 0;

	/* Not necessary, but clean */

	for(i=0; i<PRO_COM_INTQ_DEPTH; i++)
	  pro_com_intq[i] = 0;

	pro_com_a_wr0 = 0;
	pro_com_a_wr1 = 0;
	pro_com_a_wr2 = 0;
	pro_com_a_wr3 = 0;
	pro_com_a_wr4 = 0;
	pro_com_a_wr5 = 0;
	pro_com_a_wr6 = 0;
	pro_com_a_wr7 = 0;
	pro_com_a_rr0 = PRO_COM_TU | PRO_COM_TBMT;
	pro_com_a_rr1 = PRO_COM_AS;
	pro_com_a_rr2 = 0;
	pro_com_b_wr0 = 0;
	pro_com_b_wr1 = 0;
	pro_com_b_wr2 = 0; /* also readable as b_rr2 */
	pro_com_mcr0 = 0;
	pro_com_mcr1 = 0;
	pro_com_baud = 0;
	pro_com_wdata = 0;

	/* Schedule receive character polling event */

	pro_eq_sched(PRO_EVENT_COM_POLL, PRO_EQ_COM_POLL);

	/* Open serial port */

	pro_com_open();
}

void pro_com_open ()
{
	/* Open serial device */

	pro_com->reset(PRO_SER_COM, pro_com_port);

	/* Set and get serial line parameters */

	pro_com_ctrl_put();
	pro_com_ctrl_get();
}

void pro_com_exit ()
{
	pro_com->exit(PRO_SER_COM);
}
#endif
