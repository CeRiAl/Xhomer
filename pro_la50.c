/* pro_la50.c: LA50 printer emulator

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
	-speed up emu while printing PCL
*/

#ifdef PRO
#include "pdp11_defs.h"

#define SIX_RPT		0x21
#define SIX_NL		0x2d
#define SIX_CR		0x24
#define SIX_SUB		0x1a

#define	ESC		0x1b
#define FF		0x0c

#define PWIDTH		85	/* paper width in inches*10 */

#define MAXOUTRES	600

#define	MAXCOLS		(PWIDTH*MAXOUTRES/10)
#define	MAXCOLBYTES	((MAXCOLS+7)/8)

#define	S_IDLE		0
#define	S_ESC		1
#define	S_WAIT_ESC	2
#define	S_SIX_IDLE	3
#define	S_SIX_ESC	4
#define	S_SIX_RPT	5

#define	PTR_FIFO_DEPTH	(MAXCOLBYTES*6*8)


/* LA50 code entry points */

struct sercall pro_la50 = {&pro_la50_get, &pro_la50_put,
	                   &pro_la50_ctrl_get, &pro_la50_ctrl_put,
	                   &pro_la50_reset, &pro_la50_exit};


int			pro_la50_dpi = 300;	/* actual output resolution in DPI */

LOCAL unsigned char	tmpstr[256];

LOCAL int		col;
LOCAL int		maxcols, maxpos;

LOCAL int		state;
LOCAL int		esc_cmd;

LOCAL int		rpt;

LOCAL int		colrcnt, rowrcnt;

LOCAL unsigned char	pic[6][MAXCOLBYTES];


LOCAL int		ptr_fifo_h;
LOCAL int		ptr_fifo_t;
LOCAL int		ptr_fifo_level;
LOCAL unsigned char	ptr_fifo[PTR_FIFO_DEPTH];


/* Put character in FIFO */

void ptr_fifo_put (int c)
{
	ptr_fifo[ptr_fifo_h] = (unsigned char)c;
	ptr_fifo_h++;
	if (ptr_fifo_h == PTR_FIFO_DEPTH)
	  ptr_fifo_h = 0;

	ptr_fifo_level++;
}


void ptr_fifo_printf (unsigned char *s)
{
	while(*s != '\0')
	{
	  ptr_fifo_put((int)*s);
	  s++;
	}
}


/* Read character at head of FIFO, but don't advance
   head pointer */

int ptr_fifo_get ()
{
	int c;
	
	/* Check if FIFO is empty */

	if (ptr_fifo_h == ptr_fifo_t)	
	  c = PRO_NOCHAR;
	else
	{
	  c = (int)ptr_fifo[ptr_fifo_t];
	  ptr_fifo_level--;
	}

	return c;
}


/* Advance FIFO head pointer */

void ptr_fifo_advance ()
{
	ptr_fifo_t++;
	if (ptr_fifo_t == PTR_FIFO_DEPTH)
	  ptr_fifo_t = 0;
}


void start_pcl()
{
	sprintf(tmpstr, "%c*t%dR%c*r0A", ESC, pro_la50_dpi, ESC);
	ptr_fifo_printf(tmpstr);

	colrcnt = 0;
	rowrcnt = 0;

	col = 0;
	maxcols = 0;
	memset(pic, 0, 6*MAXCOLBYTES);
}


void end_pcl()
{
	sprintf(tmpstr, "%c*rB", ESC);
	ptr_fifo_printf(tmpstr);
}


void output_pcl()
{
int	ocol8, ocolmax, orow;
int	ipos, opos, pix;
int	rep, res;

unsigned char	oline[MAXCOLBYTES];

/* replication factors for 300 & 600 dpi */

int	colrep[2][2] = {{1, 2}, {3, 3}};
int	rowrep[2][4] = {{4, 4, 4, 3}, {8, 7, 8, 7}};


	if (pro_la50_dpi == 300)
	  res = 0;
	else
	  res = 1;

	for(orow=0; orow<6; orow++)
	{
	  /* Scale line up */

	  opos = 0;

	  memset(oline, 0, MAXCOLBYTES);

	  /* XXX make this maxcols */

	  for(ipos=0; ipos<MAXCOLS; ipos++)
	  {
	    pix = (pic[orow][ipos/8] >> (7-(ipos%8))) & 1;

	    for(rep=0; rep<colrep[res][colrcnt]; rep++)
	    {
	      if (opos<maxpos)
	        oline[opos/8] |= pix << (7-(opos%8));
	      opos++;
	    }

	    colrcnt++;
	    if (colrcnt == 2)
	      colrcnt = 0;
	  }

	  for(ocolmax=MAXCOLBYTES-1; ocolmax>0; ocolmax--)
	    if (oline[ocolmax] != 0)
	      break;

	  ocolmax++;

	  for(rep=0; rep<rowrep[res][rowrcnt]; rep++)
	  {
  	    sprintf(tmpstr, "%c*b%dW", ESC, ocolmax);
	    ptr_fifo_printf(tmpstr);

  	    for(ocol8=0; ocol8<ocolmax; ocol8++)
	      ptr_fifo_put((int)oline[ocol8]);
	  }

	  rowrcnt++;
	  if (rowrcnt == 4)
	    rowrcnt = 0;
	}
}


void plot_six(int c, int rep)
{
int	sixel, prow, pcol, pcolb, pcolb_r;

	sixel = c - 0x3f;

	for(pcol=col; pcol<(col+rep); pcol++)
	{
	  if (pcol < MAXCOLS)
	  {
	    pcolb = pcol / 8;
	    pcolb_r = pcol % 8;

	    for(prow=0; prow<6; prow++)
	    {
	      pic[prow][pcolb] |= ((sixel >> prow) & 1) << (7-pcolb_r);
	    }

	    maxcols++;
	  }
	}

	col += rep;
}


void six_filter(int c)
{
	switch(state)
	{
	  case S_IDLE:
	    if (c == ESC)
	      state = S_ESC;
	    else
	      ptr_fifo_put(c);
	    break;

	  case S_ESC:
	    if (c < 32)
	      state = S_IDLE;
	    else
	    {
	      esc_cmd = c;
	      state = S_WAIT_ESC;
	    }
	    break;

	  case S_WAIT_ESC:
	    if (((c >= (int)'A') && (c <= (int)'Z'))
	       || ((c >= (int)'a') && (c <= (int)'z'))
	       || (c == (int)'<') || (c == (int)'}'))
	    {
	      if ((c == (int)'q') && (esc_cmd == (int)'P'))
	      {
	        start_pcl();
	        state = S_SIX_IDLE;
	      }
	      else
	        state = S_IDLE;
	    }
	    break;

	  case S_SIX_IDLE:
	    if ((c>=0x3f) && (c<=0x7e))
	    {
	      plot_six(c, 1);
	      break;
	    }
	    else if (c == ESC)
	    {
	      state = S_SIX_ESC;
	      break;
	    }
	    else if (c == SIX_RPT)
	    {
	      rpt = 0;
	      state = S_SIX_RPT;
	      break;
	    }
	    else if (c == SIX_NL)
	    {
	      output_pcl();
	      col = 0;
	      maxcols = 0;
	      memset(pic, 0, 6*MAXCOLBYTES);
	      break;
	    }
	    else if (c == SIX_CR)
	    {
	      col = 0;
	      break;
	    }
	    else if (c == SIX_SUB)
	    {
	      plot_six(0x3f, 1);
	      break;
	    }

	  case S_SIX_ESC:
	    /* ending sequence is ESC\, but we'll take anything */

	    end_pcl();

	    state = S_IDLE;
	    break;

	  case S_SIX_RPT:
	    if (c == ESC)
	    {
	      state = S_SIX_ESC;
	      break;
	    }
	    else if ((c>(int)'9') || (c<(int)'0'))
	    {
	      if (c == SIX_SUB)
	        c = 0x3f;

	      if ((c>=0x3f) && (c<=0x7e))
	        plot_six(c, rpt);

	      state = S_SIX_IDLE;
	      break;
	    }
	    else
	    {
	      rpt = 10*rpt + c - (int)'0';
	      break;
	    }
	}
}


/* Get character from LA50 */

int pro_la50_get (int dev)
{
int	i, schar;


	/* This is a bit of a hack
	   Every time the emu does a read poll,
	   this routine polls the pcl output FIFO and copies
	   up to 10 bytes from it to the real printer hardware. */

	for(i=0; i<10; i++)
	{
	  schar = ptr_fifo_get();
	  if (schar == PRO_NOCHAR)
	    break;
	  else
	  {
	    if (pro_la50device->put(dev, schar) == PRO_FAIL)
	      break;
	    else
	      ptr_fifo_advance();
	  }
	}

	return pro_la50device->get(dev);
}


/* Send character to LA50 */

int pro_la50_put (int dev, int schar)
{
int	stat;


	if (ptr_fifo_level<PTR_FIFO_DEPTH/2)
	{
	  six_filter(schar);
	  stat = PRO_SUCCESS;
	}
	else
	  stat = PRO_FAIL;

	return stat;
}


/* Return serial line parameters */

void pro_la50_ctrl_get (int dev, struct serctrl *sctrl)
{
	pro_la50device->ctrl_get(dev, sctrl);
}


/* Set serial line parameters */

void pro_la50_ctrl_put (int dev, struct serctrl *sctrl)
{
	pro_la50device->ctrl_put(dev, sctrl);
}


/* Reset LA50 */

void pro_la50_reset (int dev, int portnum)
{
	state = S_IDLE;
	ptr_fifo_h = 0;
	ptr_fifo_t = 0;
	ptr_fifo_level = 0;
	maxpos = (PWIDTH*pro_la50_dpi/10);

	pro_la50device->reset(dev, pro_la50device_port);
}


/* Exit routine */

void pro_la50_exit (int dev)
{
	pro_la50device->exit(dev);
}
#endif
