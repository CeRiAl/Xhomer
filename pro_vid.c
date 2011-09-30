/* pro_vid.c: video board

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
		-implement appropriate vbi bits
*/

#ifdef PRO
#include "pdp11_defs.h"
#include "sim_defs.h" /* For sim_gtime() */

/* XXX */

struct iolink {
	int	low;
	int	high;
	int	(*read)();
	int	(*write)(); };

extern struct iolink iotable[]; /* from pdp11_cpu */

/* 3 planes are allocated even if EBO is not present.  The two additional
   blank planes are referenced during screen refresh. */

unsigned short PRO_VRAM[3][PRO_VID_MEMSIZE/2];

unsigned char pro_vid_mvalid[(PRO_VID_MEMSIZE/2) >> PRO_VID_CLS];

      int	pro_vid_csr;
      int	pro_vid_p1c;
      int	pro_vid_scl;

      int	pro_vid_y_offset;	/* address offset for all vram accesses */

LOCAL int	pro_vid_opc;
LOCAL int	pro_vid_x;
LOCAL int	pro_vid_y;
LOCAL int	pro_vid_cnt;
LOCAL int	pro_vid_pat;
LOCAL int	pro_vid_mbr;

LOCAL int	pro_vid_mem_base;

LOCAL int	pro_vid_plane_en;	/* This is a composite of the 3 enable bits */

LOCAL int	pro_vid_skip;		/* window update cycle skip counter */


/* Vertical retrace event scheduler */

LOCAL void pro_vid_sched ()
{
int	interval;

	if ((pro_vid_csr & PRO_VID_LM) == 0)
	  interval = PRO_EQ_NTSC;
	else
	  interval = PRO_EQ_PAL;

	pro_eq_sched(PRO_EVENT_VID, interval);
}


/* Vertical retrace event handler */

void pro_vid_eq ()
{
	/* Trigger end of frame interrupt, if enabled */

	/* XXX use better int name */

	if ((pro_vid_csr & PRO_VID_EFIE) != 0)
	  pro_int_set(PRO_INT_2A);


	/* Refresh display window, if skip count has been reached */

	if (pro_vid_skip == PRO_VID_SKIP)
	{
	  pro_screen_update();

	  pro_vid_skip = 0;
	}
	else
	  pro_vid_skip++;


	/* Schedule next event */

	pro_vid_sched();
}


/* Command done event handler */

void pro_vid_cmd_eq ()
{
	/* Trigger command done interrupt, if enabled */

	/* XXX use better int name */

	if ((pro_vid_csr & PRO_VID_DIE) != 0)
	  pro_int_set(PRO_INT_2B);

	/* Set done bit */

	pro_vid_csr = pro_vid_csr | PRO_VID_TD;

	/* Clear count */

	pro_vid_cnt = 0;
}


/* Video memory */

int pro_vram_rd (int *data, int pa, int access)
{
int	vindex;

	if ((pa & 017700000) == pro_vid_mem_base)
	{
	  vindex = vmem((pa-pro_vid_mem_base) >> 1);

	  switch (pro_vid_plane_en)
	  {
	    case 00:
	      /* XXX all disabled - return 0? */

	      *data = PRO_NOREG;
	      break;

	    case 01:
	    case 03:
	    case 05:
	    case 07:
	      *data = PRO_VRAM[0][vindex];
	      break;

	    case 02:
	    case 06:
	      *data = PRO_VRAM[1][vindex];
	      break;

	    case 04:
	      *data = PRO_VRAM[2][vindex];
	      break;

	    default:
	      *data = PRO_NOREG;
	      break;
	  }

	  /* XXX
	  printf("Video memory READ!!!!!!!!!!\r\n");
	  */
	}
	else
	{
	  *data = 0;
	  printf("%10f Warning: video memory (R) not at default location!\r\n", sim_gtime());
	}

	return SCPE_OK;
}


int pro_vram_wr (int data, int pa, int access)
{
int	vindex;

	if ((pa & 017700000) == pro_vid_mem_base)
	{
	  vindex = vmem((pa-pro_vid_mem_base) >> 1);

	  /* XXX perform byte enable check */

	  /*
	  if (access == WRITEB)
	    printf("%10f Warning: video memory byte write!\r\n", sim_gtime());
	  */
	  
	  if ((pro_vid_plane_en & 01) != 0)
	  {
	      WRITE_WB(PRO_VRAM[0][vindex], 0177777, access);
	  }

	  if ((pro_vid_plane_en & 02) != 0)
	  {
	      WRITE_WB(PRO_VRAM[1][vindex], 0177777, access);
	  }

	  if ((pro_vid_plane_en & 04) != 0)
	  {
	      WRITE_WB(PRO_VRAM[2][vindex], 0177777, access);
	  }

	  /* XXX cache is currently cleared even if no memory planes are enabled */

	  pro_vid_mvalid[cmem(vindex)] = 0;

	  /* XXX
	  printf("Video memory WRITE!!!!!!!!!!\r\n");
	  */
	}
	else
	{
	  printf("%10f Warning: video memory (W) not at default location!\r\n", sim_gtime());
	}

	return SCPE_OK;
}


/* Pixel drawing operations */

LOCAL void pro_vid_pixop(int pnum, int x, int y, int data, int pixop)
{
int	vindex, vbit, vmask, vdata, bit;

	/* Calculate starting memory index and bit offset */

	vindex = vmem((y << 6) | ((x >> 4) & 077));

	vbit = x & 017;

	vmask = ~(01 << vbit) & 0177777;

	bit = data << vbit;

	vdata = PRO_VRAM[pnum][vindex];

	/* Perform operation */

	switch (pixop)
	{
	  case PRO_VID_PIX_XOR:	/* xor pixel */
	    vdata = vdata ^ bit;
	    break;

	  case PRO_VID_PIX_SET:	/* set pixel */
	    vdata = vdata | bit;
	    break;

	  case PRO_VID_PIX_CLR:	/* clear pixel */
	    vdata = vdata & (~bit & 0177777);
	    break;

	  case PRO_VID_PIX_REP:	/* replace pixel */
	    vdata = (vdata & vmask) | bit;
	    break;

	  default:
	    break;
	}

	PRO_VRAM[pnum][vindex] = vdata;

	pro_vid_mvalid[cmem(vindex)] = 0;
}


void pro_clear_mvalid ()
{
   memset(&pro_vid_mvalid, 0, sizeof(pro_vid_mvalid));
}


/* Video registers */

int pro_vid_rd (int pa)
{
int	data;

	switch (pa & 017777776)
	{
	  case 017774400:
	    data = PRO_ID_VID;		/* IDR */
	    break;

	  case 017774404:
	    data = pro_vid_csr;
	    break;

	  case 017774406:
	    data = pro_vid_p1c;
	    break;

/* EBO */
	  case 017774410:
	    data = pro_vid_opc;
	    break;

	  case 017774414:
	    data = pro_vid_scl;
	    break;

	  case 017774416:
	    data = pro_vid_x;
	    break;

	  case 017774420:
	    data = pro_vid_y;
	    break;

/* XXX The following are write-only bits.  The real PRO returns these values */

	  case 017774422:
	    data = 0177577;
	    break;

	  case 017774424:
	    data = 0177577;
	    break;

	  case 017774426:
	    data = 0177577;
	    break;

/* EBO */
	  case 017774600:
	    data = PRO_ID_EBO;
	    break;

	  default:
	    data = PRO_NOREG;
	    break;
	}

	return data;
}

void pro_vid_wr (int data, int pa, int access)
{
int	x, y, pixop, vplane, vindex, vcount, vdata, vpixpat, vwordpat, vsi, old_csr, old_scl;
int	pixdata = 0;	/* assign dummy value to silence incorrect gcc warning */

int	vop[3];
int	voc1, voc0;

	switch (pa & 017777776)
	{
	  case 017774404:
	    old_csr = pro_vid_csr;

	    WRITE_WB(pro_vid_csr, PRO_VID_CSR_W, access);

	    /* XXX this might not be correct, but the ROM seems 
	           to imply this behavior */

	    /* Maybe it should only happen the first time after reset */

	    /* Generate TD interrupt if TD enabled and done bit set */

	    if (((pro_vid_csr & PRO_VID_DIE) != 0)
	       && ((old_csr & PRO_VID_DIE) == 0)
	       && ((pro_vid_csr & PRO_VID_TD) != 0))
	      pro_int_set(PRO_INT_2B);

	    /* Check if mapped mode changed */

	    if ((pro_vid_csr & PRO_VID_CME) != (old_csr & PRO_VID_CME))
	      pro_mapchange();

	    break;

	  case 017774406:
	    WRITE_W(pro_vid_p1c, PRO_VID_P1C_W);
	    pro_vid_plane_en = ((pro_vid_opc & PRO_VID_P3_VME) >> 11)
	                       | ((pro_vid_opc & PRO_VID_P2_VME) >> 4)
	                       | ((pro_vid_p1c & PRO_VID_P1_VME) >> 5);
	    break;

/* EBO */
	  case 017774410:
	    WRITE_W(pro_vid_opc, PRO_VID_OPC_W);
	    pro_vid_plane_en = ((pro_vid_opc & PRO_VID_P3_VME) >> 11)
	                       | ((pro_vid_opc & PRO_VID_P2_VME) >> 4)
	                       | ((pro_vid_p1c & PRO_VID_P1_VME) >> 5);
	    break;

/* EBO */
	  case 017774412:
	    /* Write to colormap */

	    pro_colormap_write(((data & PRO_VID_INDEX) >> 8), (data & PRO_VID_RGB));

	    break;

	  case 017774414:
	    old_scl = pro_vid_scl;

	    WRITE_W(pro_vid_scl, PRO_VID_SCL_W);

	    /* Call scroll routine if scl changed */

	    if (old_scl != pro_vid_scl)
	      pro_scroll();

	    /* Update memory offset */

	    pro_vid_y_offset = (pro_vid_scl << 6) & 037700;

	    break;

	  case 017774416:
	    WRITE_W(pro_vid_x, PRO_VID_X_W);
	    break;

	  case 017774420:
	    WRITE_W(pro_vid_y, PRO_VID_Y_W);
	    break;

	  case 017774422:
	    WRITE_W(pro_vid_cnt, PRO_VID_CNT_W);

	    /* Clear the done bit */

	    pro_vid_csr = pro_vid_csr & ~PRO_VID_TD;

	    /* XXX kludge - assuming plane, enabled, etc. */

	    if (pro_vid_cnt != 0)
	    {
	      /* Determine operation types for all planes */

	      vop[0] = pro_vid_p1c & PRO_VID_P1_OP;
	      vop[1] = pro_vid_opc & PRO_VID_P2_OP;
	      vop[2] = (pro_vid_opc & PRO_VID_P3_OP) >> 8;

	      /* Operation class */

	      /* XXX Make these text macros for speed */

	      voc1 = pro_vid_csr & PRO_VID_OC1; /* bit mode */

	      voc0 = pro_vid_csr & PRO_VID_OC0; /* left to right */

	      for(vplane=0; vplane<PRO_VID_NUMPLANES; vplane++)
	      {
	        x = pro_vid_x;
	        y = pro_vid_y;

	        /* Calculate memory index (for word mode) */

	        vindex = vmem((y << 6) | ((x >> 4) & 077));

	        /* Calculate pattern for word mode */

/* XXX
	        vwordpat = pro_vid_pat;
*/
	        if ((pro_vid_pat & 01) == 0)
	          vwordpat = 0;
	        else
	          vwordpat = 0177777;

	        /* Set initial pattern for pixel mode */

	        vpixpat = pro_vid_pat;

	        /* Set initial shift-in pattern for word screen shift */

	        vsi = vwordpat;

	        for(vcount = 0; vcount < pro_vid_cnt; vcount++)
	        {
	          if (voc1 == 0) /* bit mode */
	          {
	            switch (vop[vplane])
	            {
	              case PRO_VID_BM_NOP:
	                pixop = PRO_VID_PIX_NOP;
	                break;

	              case PRO_VID_BM_XOR:
	                pixop = PRO_VID_PIX_XOR;
	                pixdata = vpixpat & 01;
	                vpixpat = (vpixpat >> 1) | ((vpixpat << 15) & 0100000);
	                break;

	              case PRO_VID_BM_MOVE:
	                pixop = PRO_VID_PIX_REP;
	                pixdata = vpixpat & 01;
	                vpixpat = (vpixpat >> 1) | ((vpixpat << 15) & 0100000);
	                break;

	              case PRO_VID_BM_NMOVE:
	                pixop = PRO_VID_PIX_REP;
	                pixdata = ~vpixpat & 01;
	                vpixpat = (vpixpat >> 1) | ((vpixpat << 15) & 0100000);
	                break;

	              case PRO_VID_BM_SETPAT:
	                pixop = PRO_VID_PIX_SET;
	                pixdata = vpixpat & 01;
	                vpixpat = (vpixpat >> 1) | ((vpixpat << 15) & 0100000);
	                break;

	              case PRO_VID_BM_CLRPAT:
	                pixop = PRO_VID_PIX_CLR;
	                pixdata = vpixpat & 01;
	                vpixpat = (vpixpat >> 1) | ((vpixpat << 15) & 0100000);
	                break;

	              case PRO_VID_BM_CLRBIT:
	                pixop = PRO_VID_PIX_CLR;
	                pixdata = 01;
	                break;

	              case PRO_VID_BM_SETBIT:
	                pixop = PRO_VID_PIX_SET;
	                pixdata = 01;
	                break;

	              default:
	                pixop = PRO_VID_PIX_NOP;
	                break;
	            }

	            /* Perform the pixel operation */

	            if (pixop != PRO_VID_PIX_NOP)
	              pro_vid_pixop(vplane, x, y, pixdata, pixop);
  
	            /* Update x & y */

	            if (voc0 == 0) /* left to right */
	            {
	              x++;

	              if (x == PRO_VID_MEMWIDTH)
	              {
	                x = 0;
	                y++;

	                if (y == PRO_VID_MEMHEIGHT)
	                  y = 0;
	              }
	            }
	            else /* top to bottom */
	            {
	              y++;

	              if (y == PRO_VID_MEMHEIGHT)
	              {
	                y = 0;
	                x++;

	                if (x == PRO_VID_MEMWIDTH)
	                  x = 0;
	              }
	            }

	          }
	          else /* word mode */
	          {
	            switch (vop[vplane])
	            {
	              case PRO_VID_WM_NOP:
	                break;

	              case PRO_VID_WM_COM:
	                PRO_VRAM[vplane][vindex] = (PRO_VRAM[vplane][vindex] ^ 0177777) & 0177777;
	                break;

	              case PRO_VID_WM_MOVE:
	                PRO_VRAM[vplane][vindex] = vwordpat;
	                break;

	              case PRO_VID_WM_NMOVE:
	                PRO_VRAM[vplane][vindex] = (~vwordpat) & 0177777;
	                break;

	              /* XXX combine these three */

	              case PRO_VID_WM_S1:
	                vdata = PRO_VRAM[vplane][vindex];
	                if (voc0 == 0) /* left to right */
	                  PRO_VRAM[vplane][vindex] = ((PRO_VRAM[vplane][vindex] << 1) & 0177777) | ((vsi >> 15) & 01);
	                else /* right to left */
	                  PRO_VRAM[vplane][vindex] = ((PRO_VRAM[vplane][vindex] >> 1) & 0177777) | ((vsi << 15) & 0100000);
	                vsi = vdata;
	                break;

	              case PRO_VID_WM_S2:
	                vdata = PRO_VRAM[vplane][vindex];
	                if (voc0 == 0) /* left to right */
	                  PRO_VRAM[vplane][vindex] = ((PRO_VRAM[vplane][vindex] << 2) & 0177777) | ((vsi >> 14) & 03);
	                else /* right to left */
	                  PRO_VRAM[vplane][vindex] = ((PRO_VRAM[vplane][vindex] >> 2) & 0177777) | ((vsi << 14) & 0140000);
	                vsi = vdata;
	                break;

	              case PRO_VID_WM_S4:
	                vdata = PRO_VRAM[vplane][vindex];
	                if (voc0 == 0) /* left to right */
	                  PRO_VRAM[vplane][vindex] = ((PRO_VRAM[vplane][vindex] << 4) & 0177777) | ((vsi >> 12) & 017);
	                else /* right to left */
	                  PRO_VRAM[vplane][vindex] = ((PRO_VRAM[vplane][vindex] >> 4) & 0177777) | ((vsi << 12) & 0170000);
	                vsi = vdata;
	                break;

	              default:
	                break;
	            }

	            pro_vid_mvalid[cmem(vindex)] = 0;

	            /* Update memory index */

	            /* XXX is wrap-around handled correctly? */

	            if (voc0 == 0) /* left to right */
	              vindex = (vindex + 1) & PRO_VID_VADDR_MASK;
	            else /* right to left */
	              vindex = (vindex - 1) & PRO_VID_VADDR_MASK;

	          }

	        }
	      }

	      /* Update pattern register, once all three plane operations
                 have completed */

	      /* XXX Is this the correct behavior? */

	      pro_vid_pat = vpixpat;
	    }

	    
	    /* Schedule completion event */

	    pro_eq_sched(PRO_EVENT_VID_CMD, PRO_EQ_CMD);

	    break;

/* XXX This should only be writeable if the done bit is set */

	  case 017774424:
	    WRITE_W(pro_vid_pat, PRO_VID_PAT_W);
	    break;

	  case 017774426:
	    WRITE_W(pro_vid_mbr, PRO_VID_MBR_W);
	    pro_vid_mem_base = pro_vid_mbr << 15; /* video mem base address */

	    /* Update CPU memory decoder */

	    iotable[0].low = pro_vid_mem_base;
	    iotable[0].high = pro_vid_mem_base + PRO_VID_MEMSIZE - 1;

	    break;

	  default:
	    break;
	}
}


void pro_vid_reset ()
{
	/* Clear Video memory buffers */

        memset(&PRO_VRAM, 0, sizeof(PRO_VRAM));

	/* XXX some undefined bits have different values in the real PRO */

	pro_vid_csr = PRO_VID_CSR | PRO_VID_TD;	/* indicate command completed */
	pro_vid_p1c = 0;
	pro_vid_opc = 0;
	pro_vid_scl = 0177400;
	pro_vid_x = 0;
	pro_vid_y = 0;
	pro_vid_cnt = 0;
	pro_vid_pat = 0;
	pro_vid_mbr = 0;

	pro_vid_mem_base = 0;
	pro_vid_plane_en = 0;
	pro_vid_y_offset = 0;
	pro_vid_skip = 0;


	/* Start up vertical retrace event */

	pro_vid_sched();

	/* Reset display subsystem */

	pro_screen_reset();

	/* Open display window */

	if (pro_screen_init() == PRO_FAIL)
	  pro_exit();
}


/* Exit routine */

void pro_vid_exit ()
{
	pro_screen_close();
}
#endif
