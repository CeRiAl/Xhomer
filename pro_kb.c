/* pro_kb.c: keyboard controller

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


struct sercall		*pro_kb_orig;
LOCAL struct sercall	pro_kb_new;


/* Character I/O routines */

int pro_kb_get ()
{
int key;

	key = pro_kb_orig->get(PRO_SER_KB);

	/* Filter input characters for emulator commands */

	if (key != PRO_NOCHAR)
	  key = pro_menu(key);

	return key;
}


/* Keyboard controller registers */

int pro_kb_rd (int pa)
{
	return pro_2661kb_rd(pa);
}


void pro_kb_wr (int data, int pa, int access)
{
	pro_2661kb_wr(data, pa, access);
}


/* Intercept "get" routine and insert call to filter routine */
/* This should be called whenever pro_kb is reassigned */

void pro_kb_filter ()
{
	/* Copy original serial structure, and change kb.get link */

	pro_kb_new.get = &pro_kb_get;
	pro_kb_new.put = pro_kb->put;
	pro_kb_new.ctrl_get = pro_kb->ctrl_get;
	pro_kb_new.ctrl_put = pro_kb->ctrl_put;
	pro_kb_new.reset = pro_kb->reset;
	pro_kb_new.exit = pro_kb->exit;

	pro_kb_orig = pro_kb;
	pro_kb = &pro_kb_new;
}


void pro_kb_reset ()
{
	pro_2661kb_reset();
	pro_kb_open();
}


void pro_kb_open ()
{
	pro_kb_filter();
	pro_2661kb_open();
}


void pro_kb_exit ()
{
	pro_kb->exit(PRO_SER_KB);
}
#endif
