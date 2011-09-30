/* pro_digipad.c: digipad digitizing tablet emulator

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
		-suppressed mode crashes P/OS
		-only emulates remote and suppressed modes
		-slow down remote mode responses?
*/

#ifdef PRO
#include "pdp11_defs.h"

#define	PRO_DIGIPAD_FIFO_DEPTH	1024

#define PRO_DIGIPAD_FAR		0x04	/* pen not near tablet */
#define PRO_DIGIPAD_G		0x08	/* green button */
#define PRO_DIGIPAD_B		0x10	/* blue button */
#define PRO_DIGIPAD_R		0x20	/* red button */
#define PRO_DIGIPAD_BUTTON	0x40	/* valid button */


/* Keyboard code entry points */

struct sercall pro_digipad = {&pro_digipad_get, &pro_digipad_put,
	                      &pro_digipad_ctrl_get, &pro_digipad_ctrl_put,
	                      &pro_digipad_reset, &pro_digipad_exit};


LOCAL int	pro_digipad_s0 = -1;		/* previous commmand character */
LOCAL int	pro_digipad_s1 = -1;
LOCAL int	pro_digipad_remote = 0;		/* remote mode */
LOCAL int	pro_digipad_suppressed = 0;	/* suppressed mode */
LOCAL int	pro_digipad_enable = 0;		/* enable flow */
LOCAL int	pro_digipad_last_x = -1;	/* previous x */
LOCAL int	pro_digipad_last_y = -1;	/* previous y */
LOCAL int	pro_digipad_last_l = -1;
LOCAL int	pro_digipad_last_m = -1;
LOCAL int	pro_digipad_last_r = -1;
LOCAL int	pro_digipad_last_in = -1;

LOCAL int	pro_digipad_fifo_h = 0;
LOCAL int	pro_digipad_fifo_t = 0;
LOCAL int	pro_digipad_fifo[PRO_DIGIPAD_FIFO_DEPTH];


/* Put character in digipad FIFO */

LOCAL void pro_digipad_fifo_put (int schar)
{
int	fullness;

	/* First check if FIFO is full */

	fullness = pro_digipad_fifo_h - pro_digipad_fifo_t;

	if (fullness < 0)
	  fullness += PRO_DIGIPAD_FIFO_DEPTH;

	if (fullness < PRO_DIGIPAD_FIFO_DEPTH)
	{
	  pro_digipad_fifo[pro_digipad_fifo_h] = schar;
	  pro_digipad_fifo_h++;
	  if (pro_digipad_fifo_h == PRO_DIGIPAD_FIFO_DEPTH)
	    pro_digipad_fifo_h = 0;
	}
}


/* Calculate button and coordinate codes and load into FIFO */

void pro_digipad_coord ()
{
int	mb, tx, ty;


	/* Calculate translated X,Y coordinates */

	if (pro_digipad_remote)
	{
	  tx = 2*(pro_mouse_x-32);
	  ty = 2100 - 2*(600*pro_mouse_y/240);
	}
	else
	{
	  tx = (2200*(pro_mouse_x-32))/960;
	  ty = 2200 - (600*2200*pro_mouse_y/960)/240;
	}

	if (tx < 0)
	  tx = 0;

	/* Calculate button code */

	mb = PRO_DIGIPAD_BUTTON;

	if (pro_mouse_l)
	  mb |= PRO_DIGIPAD_R;

	if (pro_mouse_m)
	  mb |= PRO_DIGIPAD_B;

	if (pro_mouse_r)
	  mb |= PRO_DIGIPAD_G;

	if (!pro_mouse_in)
	{
	  mb |= PRO_DIGIPAD_FAR;

	  /* Digipad returns (0,0) coordinates when pen not near */

	  tx = 0;
	  ty = 0;
	}

	pro_digipad_fifo_put(mb|0x80);
	pro_digipad_fifo_put((tx&0x3f)|0x80);
	pro_digipad_fifo_put(((tx>>6)&0x3f)|0x80);
	pro_digipad_fifo_put((ty&0x3f)|0x80);
	pro_digipad_fifo_put(((ty>>6)&0x3f)|0x80);
}


/* Get character from digipad */

int pro_digipad_get (int dev)
{
	int schar;
	
	/* Check if in suppressed mode and (coordinate or button) has changed */

	if ((pro_digipad_suppressed)
	   && (pro_digipad_enable)
	   && ((pro_mouse_x != pro_digipad_last_x)
	      || (pro_mouse_y != pro_digipad_last_y)
	      || (pro_mouse_l != pro_digipad_last_l)
	      || (pro_mouse_m != pro_digipad_last_m)
	      || (pro_mouse_r != pro_digipad_last_r)
	      || (pro_mouse_in != pro_digipad_last_in)))
	{
	  pro_digipad_coord();

	  pro_digipad_last_x = pro_mouse_x;
	  pro_digipad_last_y = pro_mouse_y;
	  pro_digipad_last_l = pro_mouse_l;
	  pro_digipad_last_m = pro_mouse_m;
	  pro_digipad_last_r = pro_mouse_r;
	  pro_digipad_last_in = pro_mouse_in;
	}

	/* Check if FIFO is empty and Digipad is enabled */

	if ((pro_digipad_fifo_h == pro_digipad_fifo_t)	
	   || (!pro_digipad_enable))
	  schar = PRO_NOCHAR;
	else
	{
	  schar = pro_digipad_fifo[pro_digipad_fifo_t];
	  pro_digipad_fifo_t++;
	  if (pro_digipad_fifo_t == PRO_DIGIPAD_FIFO_DEPTH)
	    pro_digipad_fifo_t = 0;
	}

	return schar;
}


/* Send character to digipad */

int pro_digipad_put (int dev, int schar)
{
/* XXX
printf("CHAR %d!!!\r\n",schar);
*/

	/* Decode Digipad commands */

	/* XOFF */

	if (schar == 19)
	  pro_digipad_enable = 0;

	/* XON */

	else if (schar == 17)
	  pro_digipad_enable = 1;
	else
	{
	  /* Echo command character */

/* XXX should msb be set?
*/
	  pro_digipad_fifo_put(schar|0x80);

	  /* STX - request for coordinate in remote mode */

	  if ((schar == 2) && pro_digipad_remote)
	    pro_digipad_coord();

	  else if (pro_digipad_s1 == 1)
	  {
	    /* Reset Digipad */

	    if ((pro_digipad_s0 == 82) && (schar == 83))
	    {
	      pro_digipad_remote = 0;
	      pro_digipad_suppressed = 0;
	      pro_digipad_last_x = -1;
	      pro_digipad_last_y = -1;
	      pro_digipad_enable = 1;
/* XXX
*/
	      pro_digipad_fifo_put(0xff);
	      pro_digipad_fifo_put(0xff);
	      pro_digipad_fifo_put(0xff);
	      pro_digipad_fifo_put(0xff);
	      pro_digipad_fifo_put(0xff);
	    }

	    /* Remote mode */

	    else if ((pro_digipad_s0 == 82) && (schar == 77))
	    {
	      pro_digipad_remote = 1;
	      pro_digipad_suppressed = 0;
	    }

	    /* Suppressed mode */

	    else if (pro_digipad_s0 == 83)
	    {
	      pro_digipad_remote = 0;
	      pro_digipad_suppressed = 1;
	    }

	    /* Continuous/Diagnostic/Line mode unsupported */

	    else if ((pro_digipad_s0 == 67) || (pro_digipad_s0 == 68)
	            || (pro_digipad_s0 == 76))
	    {
	      pro_digipad_remote = 0;
	      pro_digipad_suppressed = 0;
	    }
	  }
	}

	pro_digipad_s1 = pro_digipad_s0;
	pro_digipad_s0 = schar;

	return PRO_SUCCESS;
}


/* Return serial line parameters */

void pro_digipad_ctrl_get (int dev, struct serctrl *sctrl)
{
	/* These are hardwired */

	sctrl->dsr = 1;
	sctrl->cts = 1;
	sctrl->ri = 0;
	sctrl->cd = 0;
}


/* Set serial line parameters */

void pro_digipad_ctrl_put (int dev, struct serctrl *sctrl)
{
}


/* Reset digipad */

void pro_digipad_reset (int dev, int portnum)
{
}


/* Exit routine */

void pro_digipad_exit (int dev)
{
}
#endif
