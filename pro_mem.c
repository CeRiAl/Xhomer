/* pro_mem.c: memory expansion board

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


/* XXX TBD:
		-This peripheral is not emulated yet, due to
		 lack of documentation
*/

#ifdef PRO
#include "pdp11_defs.h"


	unsigned short MEMROM[1024];

LOCAL	int	pro_mem_ptr;
LOCAL	int	pro_mem_cmd;


/* Memory board registers */
/* Addresses 17775000-17775002 */

int pro_mem_rd (int pa)
{
int data;

	switch (pa & 017777776)
	{
	  case 017775000:
	    data = MEMROM[pro_mem_ptr++];
	    pro_mem_ptr &= 01777;
	    break;

	  case 017775004:
	    data = 020;
	    break;

	  case 017775006:
	    data = pro_mem_cmd;
	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_mem_wr (int data, int pa, int access)
{
	switch (pa & 017777776)
	{
	  case 017775002:
	    /* Reset memory pointer */

	    pro_mem_ptr = 0;
	    break;

	  case 017775004:
	    break;

	  case 017775006:
	    WRITE_WB(pro_mem_cmd, PRO_MEM_CMD_W, access);
	    break;

	  default:
	    break;
	}
}


void pro_mem_reset ()
{
	pro_mem_ptr = 0;
	pro_mem_cmd = 040;
}


void pro_mem_exit ()
{
}
#endif
