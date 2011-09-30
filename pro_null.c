/* pro_null.c: null serial device

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
*/

#ifdef PRO
#include "pdp11_defs.h"


/* Null code entry points */

struct sercall pro_null = {&pro_null_get, &pro_null_put,
	                   &pro_null_ctrl_get, &pro_null_ctrl_put,
	                   &pro_null_reset, &pro_null_exit};


/* Get character from null */

int pro_null_get (int dev)
{
	return PRO_NOCHAR;
}


/* Send character to null */

int pro_null_put (int dev, int schar)
{
	return PRO_SUCCESS;
}


/* Return serial line parameters */

void pro_null_ctrl_get (int dev, struct serctrl *sctrl)
{
	/* These are hardwired */

	sctrl->dsr = 0;
	sctrl->cts = 0;
	sctrl->ri = 0;
	sctrl->cd = 0;
}


/* Set serial line parameters */

void pro_null_ctrl_put (int dev, struct serctrl *sctrl)
{
}


/* Reset null */

void pro_null_reset (int dev, int portnum)
{
}


/* Exit routine */

void pro_null_exit (int dev)
{
}
#endif
