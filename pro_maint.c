/* pro_maint.c: Maintenance terminal interface

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


/* This module implements the registers of the maintenance terminal
 * interface.  On a real Pro, this interface becomes available to
 * software if pins 8 and 9 on the printer port are shorted.  Here,
 * shorting pins 8 and 9 is simulated by setting the "maint_mode"
 * variable to "on" in the configuration file.
 */


#ifdef PRO
#include "pdp11_defs.h"

int	pro_maint_mode = 0;	/* set to 1 if printer port is in maintenance mode */


/* Maintenance terminal registers */

int pro_maint_rd (int pa)
{
int	data;

	/* Only allow access to these registers if the Pro is in maintenance mode */

	if (pro_maint_mode == 1)
	  switch (pa & 06)
	  {

	    case 00:
	      /* Maintenance terminal receiver CSR (RCSR) */

	      data = (pro_ptr_rd(02) & PRO_2661_RD) ? PRO_MAINT_RD : 0;
	      break;

	    case 02:
	      /* Maintenance terminal receiver register (RBUF) */

	      data = pro_ptr_rd(00);
	      break;

	    case 04:
	      /* Maintenance terminal transmitter CSR (XCSR) */

	      data = (pro_ptr_rd(2) & PRO_2661_TR) ? PRO_MAINT_TR : 0;
	      break;

	    case 06:

	      /* Maintenance terminal transmitter register (write-only) */

	      data = 0;
	      break;

	    default:
	      data = 0;
	      break;
	  }
	else
	  data = 0;

	return data;
}


void pro_maint_wr (int data, int pa, int access)
{
	/* Only allow access to these registers if the Pro is in maintenance mode */

	if (pro_maint_mode == 1)
	  switch (pa & 06)
	  {
	    case 00:
	    case 02:
	    case 04:
	      /* Maintenance terminal read-only registers */

	      break;

	    case 06:
	      /* Maintenance terminal transmitter register (XBUF) */

	      pro_ptr_wr(data, 00, access);
	      break;
	  }
}
#endif
