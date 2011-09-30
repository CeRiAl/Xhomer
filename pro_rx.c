/* pro_rx.c: RX50 floppy controller

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
		-writing a write-protected disk under DCL returns
		 different error than real PRO
		-check byte enables?
		-implement write sector command with error checks
*/

#ifdef PRO
#include "pdp11_defs.h"
#include "sim_defs.h" /* For sim_gtime() */
#include <sys/stat.h>
#include <unistd.h>

#define PRO_RX_TRACKS	80	/* tracks per disk */
#define PRO_RX_SPT	10	/* sectors per track */
#define PRO_RX_BPS	512	/* bytes per sector */
#define PRO_RX_SIZE	(PRO_RX_TRACKS*PRO_RX_SPT*PRO_RX_BPS)


char		*pro_rx_dir[4];
char		*pro_rx_file[4];

int		pro_rx_closed[4] = {0,0,0,0};	/* initial floppy door status */

LOCAL FILE	*pro_rx_fptr[4];

LOCAL unsigned char PRO_RX_SECBUF[PRO_RX_BPS];

LOCAL int	pro_rx_csr0_c;			/* csr0 for command mode */
LOCAL int	pro_rx_csr1_c;
LOCAL int	pro_rx_csr2_c;
LOCAL int	pro_rx_csr3_c;
LOCAL int	pro_rx_csr5_c;

LOCAL int	pro_rx_csr0_s;			/* csr0 for maint/status */
LOCAL int	pro_rx_csr1_s;
LOCAL int	pro_rx_csr2_s;
LOCAL int	pro_rx_csr3_s;
LOCAL int	pro_rx_csr4_s;

LOCAL int	pro_rx_secbuf_addr;
LOCAL int	pro_rx_intb_en;			/* enable interrupt B */

LOCAL int	pro_rx_exist[4];		/* existence of 4 floppy drives */
LOCAL int	pro_rx_ready[4] = {0,0,0,0};	/* diskette present */
LOCAL int	pro_rx_wprot[4] = {0,0,0,0};	/* write protection status */
LOCAL int	pro_rx_vchanged[4];		/* volume changed bits */


/* Volume-changed interrupt generator */

LOCAL void pro_rx_intb ()
{
	/* Check if intb is enabled */

	/* XXX use better int name */

	if (pro_rx_intb_en == 1)
	  pro_int_set(PRO_INT_1B);

	/* Disable generation of future interrupts until status is read */

	pro_rx_intb_en = 0;
}


/* Open floppy door */

void pro_rx_open_door (int disknum)
{
	if (pro_rx_ready[disknum] == 1)
	{
	  /* XXX */

	  printf("Open floppy %d\r\n", disknum);

	  /* Close image file */

	  fclose(pro_rx_fptr[disknum]);

	  pro_rx_ready[disknum] = 0;

	  pro_rx_wprot[disknum] = 0;

	  pro_rx_vchanged[disknum] = 1;

	  /* Generate interrupt, if needed */

	  pro_rx_intb();
	}
	else
	  printf("Floppy %d already open!\r\n", disknum);
}


/* Close floppy door */

void pro_rx_close_door (int disknum)
{
struct stat	statbuf;
char		*fname;


	if (pro_rx_ready[disknum] == 0)
	{
	  /* Construct filename */

	  fname = malloc(strlen(pro_rx_dir[disknum]) + strlen(pro_rx_file[disknum]) + 1);
	  strcpy(fname, pro_rx_dir[disknum]);
	  strcat(fname, pro_rx_file[disknum]);

	  /* Get file status */

	  stat(fname, &statbuf);

	  /* Check if writeable, and open new image file */

	  if ((statbuf.st_mode & S_IWUSR) != 0)
	    pro_rx_fptr[disknum] = fopen(fname, "r+");
	  else
	  {
	    pro_rx_fptr[disknum] = fopen(fname, "r");
	    pro_rx_wprot[disknum] = 1;
	  }

	  if (pro_rx_fptr[disknum] != NULL)
	  {
	    /* Check if image is correct size */

	    if (statbuf.st_size == PRO_RX_SIZE)
	    {
	      /* XXX */

	      printf("Close floppy %d\r\n", disknum);

	      pro_rx_ready[disknum] = 1;

	      pro_rx_vchanged[disknum] = 1;

	      /* Generate interrupt, if needed */

	      pro_rx_intb();
	    }
	    else
	    {
	      printf("Floppy %d image filesize %d incorrect! (should be %d)\r\n",
	              disknum, (int)statbuf.st_size, PRO_RX_SIZE);
	      pro_rx_wprot[disknum] = 0;
	      fclose(pro_rx_fptr[disknum]);
	    }
	  }
	  else
	  {
	    printf("Floppy %d close failed!\r\n", disknum);
	    pro_rx_wprot[disknum] = 0;
	  }

	  free(fname);
	}
	else
	  printf("Floppy %d already closed!\r\n", disknum);
}


/* Set maintenance status registers */

LOCAL void pro_rx_set_maintstat ()
{
int	disknum;

	disknum = (pro_rx_csr0_c & PRO_RX_DISKNUM) >> 1;

	pro_rx_csr0_s = (pro_rx_csr0_c & (PRO_RX_FUNC | PRO_RX_DISKNUM)) | PRO_RX_DONE;

	pro_rx_csr1_s = 0;

	pro_rx_csr2_s = pro_rx_csr1_c;

	pro_rx_csr3_s = (pro_rx_vchanged[3] << 7)
	                | (pro_rx_vchanged[2] << 6)
	                | (pro_rx_vchanged[1] << 5)
	                | (pro_rx_vchanged[0] << 4)
	                | (pro_rx_wprot[disknum] << 3)
	                | (pro_rx_ready[disknum] << 2)
	                | pro_rx_exist[disknum];

	pro_rx_csr4_s = (pro_rx_exist[3] << 6)
	                | (pro_rx_exist[2] << 4)
	                | (pro_rx_exist[1] << 2)
	                | pro_rx_exist[0];
}


/* Set read/write status registers */

LOCAL void pro_rx_set_rwstat ()
{
	pro_rx_csr0_s = (pro_rx_csr0_c & (PRO_RX_FUNC | PRO_RX_DISKNUM)) | PRO_RX_DONE;

	pro_rx_csr1_s = 0;

	pro_rx_csr2_s = pro_rx_csr1_c;

	pro_rx_csr3_s = pro_rx_csr2_c;

	pro_rx_csr4_s = 0;
}


/* Read sector */

LOCAL void pro_rx_readsec ()
{
int	i, disk, track, sector, offset;

	disk = (pro_rx_csr0_c & PRO_RX_DISKNUM) >> 1;

	track = pro_rx_csr1_c;

	sector = pro_rx_csr2_c-1;

	offset = (track*PRO_RX_SPT+sector)*PRO_RX_BPS;

	/* XXX */

/*
	printf("READ sector: disk %d track %d sector %d!\r\n", disk, track, sector);
*/

	/* Perform error checks */

	if ((pro_rx_exist[disk] != 1) || (pro_rx_ready[disk] != 1))
	  pro_rx_csr1_s = 0220; /* unavailable diskette */
	else if (track >= PRO_RX_TRACKS)
	  pro_rx_csr1_s = 0040; /* unspecified track number */
	else if ((sector < 0) || (sector >= PRO_RX_SPT))
	  pro_rx_csr1_s = 0270; /* unspecified sector number */
	else
	{
	  /* Read sector */

	  fseek(pro_rx_fptr[disk], offset, 0);

	  for(i=0; i<PRO_RX_BPS; i++)
	    PRO_RX_SECBUF[i] = getc(pro_rx_fptr[disk]);
	}
}


/* Write sector */

LOCAL void pro_rx_writesec ()
{
int	i, disk, track, sector, offset;

	disk = (pro_rx_csr0_c & PRO_RX_DISKNUM) >> 1;

	track = pro_rx_csr1_c;

	sector = pro_rx_csr2_c-1;

	offset = (track*PRO_RX_SPT+sector)*PRO_RX_BPS;

	/* XXX */

/*
	printf("WRITE sector: disk %d track %d sector %d!\r\n", disk, track, sector);
*/

	/* Perform error checks */

	if ((pro_rx_exist[disk] != 1) || (pro_rx_ready[disk] != 1))
	  pro_rx_csr1_s = 0220; /* unavailable diskette */
	else if (pro_rx_wprot[disk] == 1)
	  pro_rx_csr1_s = 0260; /* write protected */
	else if (track >= PRO_RX_TRACKS)
	  pro_rx_csr1_s = 0040; /* unspecified track number */
	else if ((sector < 0) || (sector >= PRO_RX_SPT))
	  pro_rx_csr1_s = 0270; /* unspecified sector number */
	else
	{
	  /* Write sector */

	  fseek(pro_rx_fptr[disk], offset, 0);

	  for(i=0; i<PRO_RX_BPS; i++)
	    putc(PRO_RX_SECBUF[i], pro_rx_fptr[disk]);

	  fflush(pro_rx_fptr[disk]);
	}
}


/* Start Command */

LOCAL void pro_rx_start_command(void)
{
int	i;

	switch ((pro_rx_csr0_c & PRO_RX_FUNC) >> 4)
	{
	  case PRO_RX_CMD_STAT:
	    pro_rx_set_maintstat();

	    /* Reset volume change bits */

	    for(i=0; i<4; i++)
	      pro_rx_vchanged[i] = 0;

	    /* Enable generation of volume changed interrupt */

	    pro_rx_intb_en = 1;

	    break;

	  case PRO_RX_CMD_MAINT:
	    pro_rx_set_maintstat();

	    break;

	  case PRO_RX_CMD_RESTORE:
	    pro_rx_set_maintstat();

	    break;

	  case PRO_RX_CMD_INIT:
	    pro_rx_set_maintstat();

	    break;

	  case PRO_RX_CMD_READ:
	    pro_rx_set_rwstat();

	    pro_rx_readsec();

	    break;

	  case PRO_RX_CMD_EXTEND:
	    switch (pro_rx_csr5_c & PRO_RX_EXTEND)
	    {
	      case PRO_RX_CMD_RETRY:
	        pro_rx_set_rwstat();

	        pro_rx_readsec();

	        break;

	      case PRO_RX_CMD_DELETE:
	        pro_rx_set_rwstat();

	        pro_rx_writesec();

	        break;

	      case PRO_RX_CMD_RFORMAT:
	        /* XXX */

	        /* Update csr2, etc. */

	        printf("Read format not implemented in RX50!\r\n");

	        break;

	      case PRO_RX_CMD_SFORMAT:
	        /* XXX */

	        /* Read csr2, etc. */

	        printf("Set format not implemented in RX50!\r\n");

	        break;

	      case PRO_RX_CMD_VERSION:
	        pro_rx_csr2_s = PRO_RX_VERSION;

	        break;

	      case PRO_RX_CMD_COMPARE:
	        /* XXX */

	        printf("Read and compare not implemented in RX50!\r\n");

	        break;

	      default:
	        printf("Warning: unknown extended function command in RX50!\r\n");

	        break;
	    }

	    break;

	  case PRO_RX_CMD_ADDR:
	    pro_rx_set_rwstat();

	    /* XXX */
	    printf("%10.0f Warning: read address command not implemented in RX50!\r\n", sim_gtime());
	    break;

	  case PRO_RX_CMD_WRITE:
	    pro_rx_set_rwstat();

	    pro_rx_writesec();

	    break;

	  default:
	    break;
	}

	/* XXX use better int name */

	pro_int_set(PRO_INT_1A);
}


/* RX registers */

int pro_rx_rd (int pa)
{
int	data;

	switch (pa & 017777776)
	{
	  case 017774200:
	    data = PRO_ID_RX;
	    break;

	  case 017774204:
	    data = pro_rx_csr0_s;
	    break;

	  case 017774206:
	    data = pro_rx_csr1_s;
	    break;

	  case 017774210:
	    data = pro_rx_csr2_s;
	    break;

	  case 017774212:
	    data = pro_rx_csr3_s;
	    break;

	  case 017774214:
	    data = pro_rx_csr4_s;
	    break;

	  case 017774216:
	    /* XXX correct? */

	    data = pro_rx_csr5_c;
	    break;

	  case 017774220:
	    /* Empty data buffer */

	    data = PRO_RX_SECBUF[pro_rx_secbuf_addr];

	    pro_rx_secbuf_addr++;

	    if (pro_rx_secbuf_addr == PRO_RX_BPS)
	      pro_rx_secbuf_addr = 0;

	    break;

	  case 017774222:
	    /* Clear sector buffer address */

	    pro_rx_secbuf_addr = 0;

	    /* XXX what does the real PRO return? */

	    data = 0;
	    break;

	  case 017774224:
	    /* Start command */

	    pro_rx_start_command();

	    data = 0;
	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_rx_wr (int data, int pa, int access)
{
	switch (pa & 017777776)
	{
	  case 017774204:
	    WRITE_WB(pro_rx_csr0_c, PRO_RX_CSR0_C_W, access);
	    break;

	  case 017774206:
	    WRITE_WB(pro_rx_csr1_c, PRO_RX_CSR1_C_W, access);
	    break;

	  case 017774210:
	    WRITE_WB(pro_rx_csr2_c, PRO_RX_CSR2_C_W, access);
	    break;

	  case 017774212:
	    WRITE_WB(pro_rx_csr3_c, PRO_RX_CSR3_C_W, access);
	    break;

	  case 017774216:
	    WRITE_WB(pro_rx_csr5_c, PRO_RX_CSR5_C_W, access);
	    break;

	  case 017774222:
	    /* Clear sector buffer address */

	    pro_rx_secbuf_addr = 0;

	    break;

	  case 017774224:
	    /* Start command */

	    pro_rx_start_command();

	    break;

	  /* The PRO technical manual specifies 17774226.
	     Venix 1.0/2.0  use this address.
	     However, P/OS 3.2 uses 17774220. */

	  case 017774220:
	  case 017774226:
	    /* Fill sector buffer */

	    PRO_RX_SECBUF[pro_rx_secbuf_addr] = (data & 0377);

	    pro_rx_secbuf_addr++;

	    if (pro_rx_secbuf_addr == PRO_RX_BPS)
	      pro_rx_secbuf_addr = 0;

	    break;

	  default:
	    break;
	}
}

void pro_rx_reset ()
{
int	i;


	/* Clear the 512-byte sector buffer */

        memset(&PRO_RX_SECBUF, 0, sizeof(PRO_RX_SECBUF));

	pro_rx_secbuf_addr = 0;

	pro_rx_csr0_c = 0;
	pro_rx_csr1_c = 0;
	pro_rx_csr2_c = 0;
	pro_rx_csr3_c = 0;
	pro_rx_csr5_c = 0;

	pro_rx_csr0_s = PRO_RX_DONE;
	pro_rx_csr1_s = 0;
	pro_rx_csr2_s = 0;
	pro_rx_csr3_s = 0;
	pro_rx_csr4_s = 0;

	/* XXX The following is hardcoded for now */

	pro_rx_exist[0] = 1;
	pro_rx_exist[1] = 1;
	pro_rx_exist[2] = 0;
	pro_rx_exist[3] = 0;

	pro_rx_vchanged[0] = 0;
	pro_rx_vchanged[1] = 0;
	pro_rx_vchanged[2] = 0;
	pro_rx_vchanged[3] = 0;

	pro_rx_intb_en = 0;

	/* Open floppy image files by "closing" door */

	for(i=0; i<4; i++)
	  if (pro_rx_closed[i] == 1)
	    pro_rx_close_door(i);
}


/* Exit routine */

void pro_rx_exit ()
{
int	i;

	/* Close all floppy image files by "opening" door */

	for(i=0; i<4; i++)
	  if (pro_rx_ready[i] == 1)
	    pro_rx_open_door(i);
}
#endif
