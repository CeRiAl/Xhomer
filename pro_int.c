/* pro_int.c: interrupt controllers

   Copyright (c) 1997-2004, Tarik Isani (xhomer@isani.org)

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


/* XXX TBD:
		-only one routine can access intr_req at a time!
		-byte writes
		-clear highest isr command
		-rotating priority
		-clear int_req if pending interrupt goes away???
*/

#ifdef PRO
#include "pdp11_defs.h"
#include "sim_defs.h"			/* For sim_gtime() */

extern int	int_req;		/* from pdp11_cpu */

int		pro_int_irr[4];		/* Interrupt request register */
LOCAL int	pro_int_isr[4];		/* Interrupt service register */
LOCAL int	pro_int_imr[4];		/* Interrupt mask register */
LOCAL int	pro_int_acr[4];		/* Auto clear register */
LOCAL int	pro_int_mode[4];	/* Mode register (write-only) */
LOCAL int	pro_int_presel[4];	/* Write preselect */
LOCAL int	pro_int_presel_vec[4];	/* response memory preselected vector location */
LOCAL int	pro_int_vec[4][8];	/* Interrupt vectors */


/* Determine highest priority int */

LOCAL int pro_int_check(int irr, int imr)
{
int	i, intreqs;

	intreqs = irr & ((~imr) & 0377);
	if (intreqs == 0)		/* no int pending */
	  i = PRO_NO_INT;
	else
	{
	  /* Determine highest pending interrupt */
  
	  /* XXX only fixed priority mode is currently implemented */

	  for(i=0; i<=7; i++)
	  {
	    if ((intreqs & 01) == 1)
	      break;
  
	    intreqs = intreqs >> 1;
	  }
	}

	return i;
}


/* Update status of int line to processor */

LOCAL void pro_int_update(void)
{
int	intset, cnum, intnum;

	intset = 0;

	for(cnum=0; cnum<=3; cnum++)
	{
	  intnum = pro_int_check(pro_int_irr[cnum], pro_int_imr[cnum]);

	  if ((intnum != PRO_NO_INT) && ((pro_int_mode[cnum] & PRO_INT_MM) != 0)
	     && ((pro_int_mode[cnum] & PRO_INT_IM) == 0))
	    intset = 1;
	}

	/* Set or clear interrupt, as needed */

	if (intset == 1)
	  int_req = int_req | INT_PIR4;
	else
	  int_req = int_req & ~INT_PIR4;
}


/* Interrupt controller registers */
/* Addresses 17773200-17773212 */

int pro_int_rd (int pa)
{
int	data, i, cnum;

	/* Determine controller number */

	cnum = (pa >> 2) & 03;

	switch (pa & 02)
	{
	  case 00:	/* 17773200, 17773204, 17773210 */
	    switch ((pro_int_mode[cnum] >> 5) & 03)
	    {
	      case 00:
	        data = pro_int_isr[cnum];
	        break;

	      case 01:
	        data = pro_int_imr[cnum];
	        break;

	      case 02:
	        data = pro_int_irr[cnum];
	        break;

	      case 03:
	        data = pro_int_acr[cnum];
	        break;

	      default: /* To get rid of annoying gcc warning */
	        data = 0;
	        break;
	    }
	    break;

	  case 02:	/* 17773202, 17773206, 17773212 */
	    i = pro_int_check(pro_int_irr[cnum], pro_int_imr[cnum]);

	    if (i == PRO_NO_INT)
	      data = 0200;		/* no int pending, return 0 in lsbs? */
	    else
	      data = i;			/* int pending */

	    data = data | ((pro_int_mode[cnum] & 0001) << 5)
	                | ((pro_int_mode[cnum] & 0004) << 2)
	                | ((pro_int_mode[cnum] & 0200) >> 4);
	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_int_wr (int data, int pa, int access)
{
int	cnum;

/* XXX byte accesses not handled correctly yet */

	/* Determine controller number */

	cnum = (pa >> 2) & 03;

	switch (pa & 02)
	{
	  case 00:	/* 17773200, 17773204, 17773210 */
	    switch (pro_int_presel[cnum])
	    {
	      case PRO_PRESEL_IMR:
	        pro_int_imr[cnum] = data;
	        pro_int_update();
	        break;

	      case PRO_PRESEL_ACR:
	        pro_int_acr[cnum] = data;
	        break;

	      case PRO_PRESEL_RESP:
	        pro_int_vec[cnum][pro_int_presel_vec[cnum]] = data;
	        break;

	      default:
	        break;
	    }
	    break;

	  case 02:	/* 17773202, 17773206, 17773212 */
	    if ((data & 0377) == 0)	/* reset */
	    {
	      pro_int_presel[cnum] = 0;
	      pro_int_irr[cnum] = 0;
	      pro_int_isr[cnum] = 0;
	      pro_int_imr[cnum] = 0377;
	      pro_int_acr[cnum] = 0;
	      pro_int_mode[cnum] = 0;
	      pro_int_update();
	      break;
	    }
	    else
	      switch (data & 0370)
	      {
	        case 0020:		/* clear irr and imr */
	          pro_int_irr[cnum] = 0;
	          pro_int_imr[cnum] = 0;
	          pro_int_update();
	          break;

	        case 0030:		/* clear single irr and imr bit */
	          pro_clrbit(pro_int_irr[cnum], (data & 07));
	          pro_clrbit(pro_int_imr[cnum], (data & 07));
	          pro_int_update();
	          break;

	        case 0040:		/* clear imr */
	          pro_int_imr[cnum] = 0;
	          pro_int_update();
	          break;

	        case 0050:		/* clear single imr bit */
	          pro_clrbit(pro_int_imr[cnum], (data & 07));
	          pro_int_update();
	          break;

	        case 0060:		/* set imr */
	          pro_int_imr[cnum] = 0377;
	          pro_int_update();
	          break;

	        case 0070:		/* set single imr bit */
	          pro_setbit(pro_int_imr[cnum], (data & 07));
	          pro_int_update();
	          break;

	        case 0100:		/* clear irr */
	          pro_int_irr[cnum] = 0;
	          pro_int_update();
	          break;

	        case 0110:		/* clear single irr bit */
	          pro_clrbit(pro_int_irr[cnum], (data & 07));
	          pro_int_update();
	          break;

	        case 0120:		/* set irr */
	          pro_int_irr[cnum] = 0377;
	          pro_int_update();
	          break;

	        case 0130:		/* set single irr bit */
	          pro_setbit(pro_int_irr[cnum], (data & 07));
	          pro_int_update();
	          break;

	        case 0140:		/* clear highest priority isr bit*/
	        case 0150:
	          /* XXX unimplemented */
	          printf("clear highest priority isr bit!!!!!!!\r\n");
	          break;

	        case 0160:		/* clear isr */
	          pro_int_isr[cnum] = 0;
	          break;

	        case 0170:		/* clear single isr bit */
	          pro_clrbit(pro_int_isr[cnum], (data & 07));
	          break;

	        case 0200:		/* load mode bits m0 thru m4 */
	        case 0210:
	        case 0220:
	        case 0230:
	          pro_int_mode[cnum] = (pro_int_mode[cnum] & 0340) | (data & 0037);
	          pro_int_update();
/* XXX
	          if ((pro_int_mode[cnum] & PRO_INT_PM) != 0)
	            printf("%10.0f Warning: rotating priority not implemented in interrupt controller!\r\n", sim_gtime());
*/
	          break;

	        case 0240:		/* control mode bits m5 thru m7 */
	        case 0250:
	          pro_int_mode[cnum] = (pro_int_mode[cnum] & 0237) | ((data << 3) & 0140);

	          if ((data & 03) == 01)
	            pro_setbit(pro_int_mode[cnum], 7);
	          else if ((data & 03) == 02)
	            pro_clrbit(pro_int_mode[cnum], 7);
	          pro_int_update();
	          break;

	        case 0260:		/* preselect imr for writing */
	        case 0270:
	          pro_int_presel[cnum] = PRO_PRESEL_IMR;
	          break;

	        case 0300:		/* preselect acr for writing */
	        case 0310:
	          pro_int_presel[cnum] = PRO_PRESEL_ACR;
	          break;

	        case 0340:		/* preselect response memory for writing */
	          pro_int_presel[cnum] = PRO_PRESEL_RESP;
	          pro_int_presel_vec[cnum] = (data & 07);
	          break;

	        default:
	          break;
	      }
	    break;

	  default:
	    break;
	}
}

int pro_int_ack (void)
{
/* XXX handle case where no interrupt is set */
int	cnum, intnum, vec;

	for(cnum=0; cnum<=3; cnum++)
	{
	  intnum = pro_int_check(pro_int_irr[cnum], pro_int_imr[cnum]);

	  if ((intnum != PRO_NO_INT) && ((pro_int_mode[cnum] & PRO_INT_MM) != 0)
	     && ((pro_int_mode[cnum] & PRO_INT_IM) == 0))
	    break;
	}

	/* Lookup interrupt vector to return */

	if ((pro_int_mode[cnum] & PRO_INT_VS) != 0)
	  vec = pro_int_vec[cnum][0];	/* return common vector if VSS set */
	else
	  vec = pro_int_vec[cnum][intnum];

	/* Clear IRR bit */

	pro_clrbit(pro_int_irr[cnum], intnum);

	/* Set ISR bit if corresponding ACR bit is cleared */

	if (((pro_int_acr[cnum] >> intnum) & 01) == 0)
	  pro_setbit(pro_int_isr[cnum], intnum);

	/* Check if more interrupts are pending, set J11 int bit again, if so */

	pro_int_update();

	/* XXX */

	#ifdef IOTRACE
	fprintf(iotfptr, "%10.0lf Acknowledged interrupt with vector %o\r\n", sim_gtime(), vec);
	fflush(iotfptr);
	#endif

	return vec;
}

void pro_int_set (int intnum)
{
int	cnum, intbit;

	/* Extract controller number and interrupt bit */

	cnum = (intnum >> 8) & 0377;
	intbit = intnum & 0377;

	/* Set interrupt request */

	/* XXX Does master mask mask this? */

	pro_int_irr[cnum] |= intbit;

	pro_int_update();

/* XXX */
/*
printf("%o %o %o %o %o\r\n",i, cnum, pro_int_irr[cnum], pro_int_imr[cnum], pro_int_mode[cnum]);
*/
}

void pro_int_reset ()
{
int cnum;

	for(cnum=0; cnum<=3; cnum++)
	{
	  pro_int_presel[cnum] = 0;
	  pro_int_irr[cnum] = 0;
	  pro_int_isr[cnum] = 0;
	  pro_int_imr[cnum] = 0377;
	  pro_int_acr[cnum] = 0;
	  pro_int_mode[cnum] = 0;
	  pro_int_presel_vec[cnum] = 0;

	  pro_int_vec[cnum][0] = 0;
	  pro_int_vec[cnum][1] = 0;
	  pro_int_vec[cnum][2] = 0;
	  pro_int_vec[cnum][3] = 0;
	  pro_int_vec[cnum][4] = 0;
	  pro_int_vec[cnum][5] = 0;
	  pro_int_vec[cnum][6] = 0;
	  pro_int_vec[cnum][7] = 0;
	}
	
	/* Probably not necessary */

	pro_int_update();
}
#endif
