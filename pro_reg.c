/* pro_reg.c: General PRO registers

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


#ifdef PRO
#include "pdp11_defs.h"
#include "sim_defs.h" /* For sim_gtime() */

extern int	PC; /* from pdp11_cpu */

int		pro_led;
int		pro_csr;

#ifndef BUILTIN_ROMS
unsigned short	ROM[8192];
unsigned short	IDROM[32];
#else
#include "id.h"
#include "pro350.h"
#endif
unsigned short	BATRAM[50];


/* pro_rom.c: PRO 16KB Boot/diagnostic ROM, I/O address 17730000-17767777 */

int rom_rd (int *data, int pa, int access)
{
	*data = ROM[(pa-017730000) >> 1];
	return SCPE_OK;
}

int rom_wr (int data, int pa, int access)
{
	return SCPE_NXM;				/* ??? invalid access */
}


/* Standard I/O dispatch routine, many addresses */

int reg_rd (int *data, int pa, int access)
{

#ifdef IOTRACE
        if (iotfptr)
	{
	  fprintf(iotfptr, "%10.0lf ",sim_gtime());
	  fprintf(iotfptr, "%6o ",PC);
	}
#endif

	if ((pa>=017773600) && (pa<=017773677))	/* ID PROM */
	  *data = IDROM[(pa-017773600) >> 1];
	else if ((pa>=017773034) && (pa<=017773177)) /* Battery backed RAM */
	  *data = BATRAM[(pa-017773034) >> 1];
	else if ((pa>=017773200) && (pa<=017773213)) /* Int controllers */
	  *data = pro_int_rd(pa);
	else
	  switch (pa & 017777776)
	  {
	    case 017773000:
	    case 017773002:
	    case 017773004:
	    case 017773006:
	    case 017773010:
	    case 017773012:
	    case 017773014:
	    case 017773016:
	    case 017773020:
	    case 017773022:

	    case 017773024:
	    case 017773026:
	    case 017773030:
	    case 017773032:
	      *data = pro_clk_rd(pa);	/* clock */
	      break;

	    case 017773300:
	    case 017773302:
	    case 017773304:
	    case 017773306:
	    case 017773310:
	    case 017773312:
	    case 017773314:
	      *data = pro_com_rd(pa);	/* communication port */
	      break;

	    case 017773400:
	    case 017773402:
	    case 017773404:
	    case 017773406:
	      *data = pro_ptr_rd(pa);	/* printer controller */
	      break;

	    case 017773500:
	    case 017773502:
	    case 017773504:
	    case 017773506:
	      *data = pro_kb_rd(pa);	/* keyboard controller */
	      break;

	    case 017773700:
	      *data = pro_csr;
	      break;

	    case 017773702:
	      *data = PRO_OMP;	/* Option module present */
	      break;

	    case 017773704:
	      *data = pro_led;
	      break;

#if PRO_RD_PRESENT
	    case 017774000:
	    case 017774004:
	    case 017774006:
	    case 017774010:
	    case 017774012:
	    case 017774014:
	    case 017774016:
	    case 017774020:
	      *data = pro_rd_rd(pa);
	      break;
#endif

#if PRO_RX_PRESENT
	    case 017774200:
	    case 017774204:
	    case 017774206:
	    case 017774210:
	    case 017774212:
	    case 017774214:
	    case 017774216:
	    case 017774220:
	    case 017774222:
	    case 017774224:
	      *data = pro_rx_rd(pa);
	      break;
#endif

#if PRO_VID_PRESENT
	    case 017774400:
	    case 017774404:
	    case 017774406:
	    case 017774414:
	    case 017774416:
	    case 017774420:
/* EBO */
	    case 017774410:
#if PRO_EBO_PRESENT
	    case 017774600:
#endif
	      *data = pro_vid_rd(pa);
	      break;
#endif

#if PRO_MEM_PRESENT
	    case 017775000:
	    case 017775004:
	    case 017775006:
	      *data = pro_mem_rd(pa);	/* memory expansion */
	      break;
#endif

/* Option board 6 not installed
	    case 017775200:
	      break;
*/

	    case 017777560:
	    case 017777562:
	    case 017777564:
	    case 017777566:

	      *data = pro_maint_rd(pa);	/* maintenance terminal interface */
	      break;

	    case 017777750:
	      /* Processor maintenance register */

	      *data = 0;
	      break;

	    default:
	      *data = PRO_NOREG;
#ifdef IOTRACE
              if (iotfptr)
	      {
	        fprintf(iotfptr, "*R %o\n",pa);
	        fflush(iotfptr);
	      }
#endif
	      return SCPE_NXM;
	  }

#ifdef IOTRACE
        if (iotfptr)
	{
	  fprintf(iotfptr, " R %o, %o\n",pa,*data);
	  fflush(iotfptr);
	}
#endif

	return SCPE_OK;
}

int reg_wr (int data, int pa, int access)
{
	/* XXX Writes to ID PROM *should* return NXM */

#ifdef IOTRACE
        if (iotfptr)
        {
	  fprintf(iotfptr, "%10.0lf ",sim_gtime());
	  fprintf(iotfptr, "%6o",PC);
	  if (access == WRITEB)
	    fprintf(iotfptr, "B");
	  else
	    fprintf(iotfptr, " ");
	}
#endif

	if ((pa>=017773034) && (pa<=017773177)) /* Battery backed RAM */
	  WRITE_WB(BATRAM[(pa-017773034) >> 1], PRO_BATRAM_W, access);
	else if ((pa>=017773200) && (pa<=017773213)) /* Int controllers */
	  pro_int_wr(data, pa, access);
	else
	  switch (pa & 017777776)
	  {
	    case 017773000:
	    case 017773002:
	    case 017773004:
	    case 017773006:
	    case 017773010:
	    case 017773012:
	    case 017773014:
	    case 017773016:
	    case 017773020:
	    case 017773022:

	    case 017773024:
	    case 017773026:
	    case 017773030:
	    case 017773032:
	      pro_clk_wr(data, pa, access);
	      break;

	    case 017773300:
	    case 017773302:
	    case 017773304:
	    case 017773306:
	    case 017773310:
	    case 017773312:
	    case 017773314:
	      pro_com_wr(data, pa, access);
	      break;

	    case 017773400:
	    case 017773402:
	    case 017773404:
	    case 017773406:
	      pro_ptr_wr(data, pa, access);
	      break;

	    case 017773500:
	    case 017773502:
	    case 017773504:
	    case 017773506:
	      pro_kb_wr(data, pa, access);
	      break;

	    case 017773700:
	      WRITE_WB(pro_csr, PRO_CSR_W, access);
	      break;

	    case 017773704:
	      WRITE_WB(pro_led, PRO_LED_W, access);
	      break;

#if PRO_RD_PRESENT
	    case 017774002:
	    case 017774004:
	    case 017774006:
	    case 017774010:
	    case 017774012:
	    case 017774014:
	    case 017774016:
	    case 017774020:
	      pro_rd_wr(data, pa, access);
	      break;
#endif

#if PRO_RX_PRESENT
	    case 017774202:
	    case 017774204:
	    case 017774206:
	    case 017774210:
	    case 017774212:
	    case 017774216:
	    case 017774220:
	    case 017774222:
	    case 017774224:
	    case 017774226:
	      pro_rx_wr(data, pa, access);
	      break;
#endif

#if PRO_VID_PRESENT
	    case 017774402:
	    case 017774404:
	    case 017774406:
	    case 017774414:
	    case 017774416:
	    case 017774420:
	    case 017774422:
	    case 017774424:
	    case 017774426:
/* EBO */
	    case 017774410:
	    case 017774412:
#if PRO_EBO_PRESENT
	    case 017774602:
#endif
	      pro_vid_wr(data, pa, access);
	      break;
#endif

#if PRO_MEM_PRESENT
	    case 017775002:
	    case 017775004:
	    case 017775006:
	      pro_mem_wr(data, pa, access);
	      break;
#endif

/* Option board 6 not installed
	    case 017775202:
	      break;
*/

	    case 017777560:
	    case 017777562:
	    case 017777564:
	    case 017777566:
	      pro_maint_wr(data, pa, access);	/* maintenance terminal interface */
	      break;

	    case 017777750:
	      /* Processor maintenance register */

	      break;

	    default:
#ifdef IOTRACE
              if (iotfptr)
              {
	        fprintf(iotfptr, "*W %o, %o\n",pa,data);
	        fflush(iotfptr);
	      }
#endif
	      return SCPE_NXM;
	  }

#ifdef IOTRACE
        if (iotfptr)
        {
	  fprintf(iotfptr, " W %o, %o\n",pa,data);
	  fflush(iotfptr);
	}
#endif

	return SCPE_OK;
}
#endif
