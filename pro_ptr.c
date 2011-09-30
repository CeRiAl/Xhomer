/* pro_ptr.c: printer controller

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


/* Printer controller registers */

int pro_ptr_rd (int pa)
{
	return pro_2661ptr_rd(pa);
}


void pro_ptr_wr (int data, int pa, int access)
{
	pro_2661ptr_wr(data, pa, access);
}


void pro_ptr_reset ()
{
	pro_2661ptr_reset();
	pro_ptr_open();
}


void pro_ptr_open ()
{
	pro_2661ptr_open();
}


void pro_ptr_exit ()
{
	pro_ptr->exit(PRO_SER_PTR);
}
#endif
