/* pro_config.h: system memory and peripheral selection

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


/* Base system memory size */

#define PRO_BASE_1M		1	/* 0=256K 1=1M */

/* Installed boards */

#define PRO_RD_PRESENT		1	/* RD hard disk controller */
#define PRO_RX_PRESENT		1	/* RX floppy disk controller */
#define PRO_VID_PRESENT		1	/* standard graphics board */
#define PRO_EBO_PRESENT		1	/* extended bitmap option board */
#define PRO_MEM_PRESENT		0	/* memory expansion board emulation is incomplete */
