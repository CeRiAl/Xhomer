/* pro_rd.c: RD hard drive controller

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
		-check byte enables?
		-read verify not implemented
*/

#ifdef PRO
#include "pdp11_defs.h"
#include <sys/stat.h>
#include <unistd.h>


char		*pro_rd_dir;
char		*pro_rd_file;

int		pro_rd_heads = 0;
int		pro_rd_cyls = 0;
int		pro_rd_secs = 0;

LOCAL FILE	*pro_rd_fptr = NULL;

LOCAL unsigned char PRO_RD_SECBUF[512];

LOCAL int 	pro_rd_secbuf_ptr;

LOCAL int	pro_rd_ep;
LOCAL int	pro_rd_sec;
LOCAL int	pro_rd_cyl;
LOCAL int	pro_rd_head;
LOCAL int	pro_rd_s2c;
LOCAL int	pro_rd_status;

/* Known hard drive geometries */

#define NUMGEOM	13	/* number of defined geometries */

LOCAL const int	pro_rd_geom[NUMGEOM][3] = {{4,  615, 16},	/* RD31 21M */
	                                   {4,  615, 17},
	                                   {6,  820, 16},	/* RD32 43M */
	                                   {6,  820, 17},
	                                   {4,  153, 16},	/* RD50 5M */
	                                   {4,  153, 17},
	                                   {4,  306, 16},	/* RD51 10M */
	                                   {4,  306, 17},
	                                   {8,  512, 16},	/* RD52 36M */
	                                   {8,  512, 17},
	                                   {8, 1024, 16},	/* RD53 71M */
	                                   {8, 1024, 17},
	                                   {8, 1024, 32}};	/* max 128 MB disk */


/* Trigger interrupt A */

LOCAL void pro_rd_inta ()
{
	/* Signal end of operation */

	pro_rd_status = pro_rd_status | PRO_RD_OPENDED;

	/* XXX use better int name */

	pro_int_set(PRO_INT_0A);
}


/* Trigger interrupt B */

LOCAL void pro_rd_intb ()
{
	/* Set DRQ bits */

	pro_rd_status = pro_rd_status | PRO_RD_DRQ;
	pro_rd_s2c = pro_rd_s2c | PRO_RD_DATAREQ;

	/* XXX use better int name */

	pro_int_set(PRO_INT_0B);
}


/* Signal error condition */

LOCAL void pro_rd_error ()
{
	pro_rd_status = pro_rd_status & (~PRO_RD_BUSY);
	pro_rd_s2c = pro_rd_s2c | PRO_RD_ERROR;

	/* XXX Currently only handles sector not found errors */

	pro_rd_ep = pro_rd_ep | PRO_RD_IDNF;

	/* XXX is this ok? */

	pro_rd_inta();
}


/* Event scheduler */

LOCAL void pro_rd_sched ()
{
	/* A 4-5 instruction delay is needed to pass diagnostics */

	pro_eq_sched(PRO_EVENT_RD, PRO_EQ_RD);
}

/* Event queue handler */

void pro_rd_eq ()
{
	/* Trigger interrupt */

	pro_rd_intb();
}


/* RD registers */

int pro_rd_rd (int pa)
{
int	data;

	switch (pa & 017777776)
	{
	  case 017774000:
	    data = PRO_ID_RD;
	    break;

	  case 017774004:
	    data = pro_rd_ep;
	    break;

	  case 017774006:
	    data = pro_rd_sec;
	    break;

	  case 017774010:
	    data = PRO_RD_SECBUF[pro_rd_secbuf_ptr+1] * 256
	           + PRO_RD_SECBUF[pro_rd_secbuf_ptr];
	           
	    if ((pro_rd_status & PRO_RD_DRQ) != 0)
	    {
	      if (pro_rd_secbuf_ptr < 510)
	      {
	        pro_rd_secbuf_ptr += 2;

	        /* Trigger interrupt for next transfer */

	        pro_rd_intb();
	      }
	      else
	      {
	        /* Reset data request bits */

	        pro_rd_status = pro_rd_status & (~PRO_RD_DRQ);
	        pro_rd_s2c = pro_rd_s2c & (~PRO_RD_DATAREQ);

	        /* Signal end of operation */

	        pro_rd_inta();
	      }
	    }

	    break;

	  case 017774012:
	    data = pro_rd_cyl;
	    break;

	  case 017774014:
	    data = pro_rd_head;
	    break;

	  case 017774016:
	    data = pro_rd_s2c;

	    /* XXX should OPENDED really be cleared? */

	    pro_rd_status = pro_rd_status & (~PRO_RD_OPENDED);

	    break;

	  case 017774020:
	    data = pro_rd_status;
	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_rd_wr (int data, int pa, int access)
{
int	i, head, cyl, sec, offset;

	switch (pa & 017777776)
	{
	  case 017774004:
	    WRITE_W(pro_rd_ep, PRO_RD_EP_W);
	    break;

	  case 017774006:
	    WRITE_W(pro_rd_sec, PRO_RD_SEC_W);
	    break;

	  case 017774010:
	    if ((pro_rd_status & PRO_RD_DRQ) != 0)
	    {
	      PRO_RD_SECBUF[pro_rd_secbuf_ptr] = data & 0377;
	      PRO_RD_SECBUF[pro_rd_secbuf_ptr+1] = (data & 0177400) >> 8;

	      if (pro_rd_secbuf_ptr < 510)
	      {
	        pro_rd_secbuf_ptr += 2;

	        /* Trigger interrupt for next transfer */

	        pro_rd_intb();
	      }
	      else
	      {
	        /* Perform write or format command */

	        head = pro_rd_head;

	        cyl = pro_rd_cyl;

	        sec = pro_rd_sec & PRO_RD_SEC;

	        switch (pro_rd_s2c & PRO_RD_CMD)
	        {
	          case PRO_RD_CMD_FORMAT:
	            offset = (cyl * pro_rd_heads * pro_rd_secs + head * pro_rd_secs) * 512;

	            fseek(pro_rd_fptr, offset, 0);

	            /* Use last character of secbuf as padding character */

	            for(i=0; i<(512*pro_rd_secs); i++)
	              putc(PRO_RD_SECBUF[511], pro_rd_fptr);

	            fflush(pro_rd_fptr);

	            break;

	          case PRO_RD_CMD_WRITE:
	            offset = (cyl * pro_rd_heads * pro_rd_secs + head * pro_rd_secs + sec) * 512;

	            fseek(pro_rd_fptr, offset, 0);

	            for(i=0; i<512; i+=2)
	            {
	              putc(PRO_RD_SECBUF[i], pro_rd_fptr);
	              putc(PRO_RD_SECBUF[i+1], pro_rd_fptr);
	            }

	            fflush(pro_rd_fptr);

	            break;

	          default:
	            break;
	        }

	        /* Reset data request bits */

	        pro_rd_status = pro_rd_status & (~PRO_RD_DRQ);
	        pro_rd_s2c = pro_rd_s2c & (~PRO_RD_DATAREQ);

	        /* Signal end of operation */

	        pro_rd_inta();
	      }
	    }

	    break;

	  case 017774012:
	    WRITE_W(pro_rd_cyl, PRO_RD_CYL_W);
	    break;

	  case 017774014:
	    WRITE_W(pro_rd_head, PRO_RD_HEAD_W);
	    break;

	  case 017774016:
	    WRITE_W(pro_rd_s2c, PRO_RD_S2C_W);

	    head = pro_rd_head;

	    cyl = pro_rd_cyl;

	    sec = pro_rd_sec & PRO_RD_SEC;

	    offset = (cyl * pro_rd_heads * pro_rd_secs + head * pro_rd_secs + sec) * 512;

	    /* Clear error conditions */

	    pro_rd_s2c = pro_rd_s2c & (~PRO_RD_ERROR);
	    pro_rd_ep = pro_rd_ep & (~PRO_RD_ERRORS);

	    /* Clear data transfer requests */

	    pro_rd_s2c = pro_rd_s2c & (~PRO_RD_DATAREQ);
	    pro_rd_status = pro_rd_status & (~PRO_RD_DRQ);

	    /* Clear operation ended bit */

	    pro_rd_status = pro_rd_status & (~PRO_RD_OPENDED);

	    /* Execute command */

	    switch (pro_rd_s2c & PRO_RD_CMD)
	    {
	      case PRO_RD_CMD_RESTORE:
/* XXX
	        printf("RD restore command\r\n");
*/

	        /* Set operation ended bit */

	        pro_rd_inta();

	        break;

	      case PRO_RD_CMD_READ:
/* XXX
	        printf("RD read  head = %d cyl = %d sec = %d\r\n", pro_rd_head, pro_rd_cyl, pro_rd_sec);
*/

	        if ((head >= pro_rd_heads) || (cyl >= pro_rd_cyls) || (sec >= pro_rd_secs))
	          pro_rd_error();
	        else
	        {
	          fseek(pro_rd_fptr, offset, 0);

	          for(i=0; i<512; i++)
	            PRO_RD_SECBUF[i] = getc(pro_rd_fptr);

	          pro_rd_secbuf_ptr = 0;

	          /* Schedule first DRQ */

	          pro_rd_sched();
	        }
	        break;

	      case PRO_RD_CMD_WRITE:
/* XXX
	        printf("RD write  head = %d cyl = %d sec = %d\r\n", pro_rd_head, pro_rd_cyl, pro_rd_sec);
*/

	        if ((head >= pro_rd_heads) || (cyl >= pro_rd_cyls) || (sec >= pro_rd_secs))
	          pro_rd_error();
	        else
	        {
	          pro_rd_secbuf_ptr = 0;

	          /* Trigger first DRQ interrupt */

	          pro_rd_intb();
	        }

	        break;

	      case PRO_RD_CMD_FORMAT:
/* XXX
	        printf("RD format  head = %d cyl = %d\r\n", pro_rd_head, pro_rd_cyl);
*/

	        if ((head >= pro_rd_heads) || (cyl >= pro_rd_cyls))
	          pro_rd_error();
	        else
	        {
	          pro_rd_secbuf_ptr = 0;

	          /* Trigger first DRQ interrupt */

	          pro_rd_intb();
	        }
	        break;

	      default:
	        printf("RD illegal command\r\n");
	        break;
	    }

	    break;

	  case 017774020:
	    /* XXX watch for reset command */

	    break;

	  default:
	    break;
	}
}

void pro_rd_reset ()
{
struct stat	statbuf;
int		i, fsize, gfound;
char		*fname;


	/* Construct filename */

	fname = malloc(strlen(pro_rd_dir) + strlen(pro_rd_file) + 1);
	strcpy(fname, pro_rd_dir);
	strcat(fname, pro_rd_file);

	/* Clear 512-byte sector buffer */

        memset(&PRO_RD_SECBUF, 0, sizeof(PRO_RD_SECBUF));

	/* Open disk image file */

        if (pro_rd_fptr) fclose(pro_rd_fptr);
	pro_rd_fptr = fopen(fname, "r+");

	gfound = 0;

	if (pro_rd_fptr == NULL)
	  printf("Unable to open rd image %s\n", fname);
	else
	{
	  /* Get filesize */

	  stat(fname, &statbuf);

	  fsize = (int)statbuf.st_size;

	  /* Check if geometry was forced in config file */

	  if ((pro_rd_heads != 0) || (pro_rd_cyls != 0) || (pro_rd_secs != 0))
	  {
	    if (pro_rd_heads*pro_rd_cyls*pro_rd_secs*512 == fsize)
	      gfound = 1;
	    else
	      printf("Geometry %d %d %d does not match rd filesize %d\n",
	              pro_rd_heads, pro_rd_cyls, pro_rd_secs, fsize);
	  }
	  else

	  /* Scan geometry table for a match */

	  {
	    for(i=0; i<NUMGEOM; i++)
	      if (pro_rd_geom[i][0]*pro_rd_geom[i][1]
	          *pro_rd_geom[i][2]*512 == fsize)
	      {
	        pro_rd_heads = pro_rd_geom[i][0];
	        pro_rd_cyls = pro_rd_geom[i][1];
	        pro_rd_secs = pro_rd_geom[i][2];
	        gfound = 1;
	        break;
	      }

	    if (gfound == 0)
	      printf("Unable to determine rd geometry for filesize %d bytes\n", fsize);
	  }

	  /* Close image file if geometry does not match filesize */

	  if (gfound == 0)
	    fclose(pro_rd_fptr);
	}

	if (gfound == 0)
	{
	  pro_rd_heads = 0;
	  pro_rd_cyls = 0;
	  pro_rd_secs = 0;
	}

	pro_rd_secbuf_ptr = 0;

	pro_rd_ep = 0;
	pro_rd_sec = 0;
	pro_rd_cyl = 0;
	pro_rd_head = 0;
	pro_rd_s2c = PRO_RD_DRDY | PRO_RD_SEEKC; /* XXX plus SEEKC? */
	pro_rd_status = PRO_RD_OPENDED;

	free(fname);
}


/* Exit routine */

void pro_rd_exit ()
{
        if (pro_rd_fptr!=NULL)
        {
          fclose(pro_rd_fptr);
          pro_rd_fptr = NULL;
	}
}
#endif
