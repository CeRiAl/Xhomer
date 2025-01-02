/* pro_serial.c: serial port

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
	-return line parameters:
		break
		parity error
	-unsupported baud rates: 2000, 3600, 7200
	-unsupported stop bits: 1.5
	-parity errors are ignored
	-only sends momentary break instead of holding break
	-implement WDEPTH in way that works for parallel port
*/

#ifdef PRO
#include "pdp11_defs.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#ifdef __hpux__
#include <sys/modem.h>
#endif

#define PRO_SER_RSIZE	16	/* number of characters to read at once */


/* Serial code entry points */

struct sercall pro_serial = {&pro_serial_get, &pro_serial_put,
	                     &pro_serial_ctrl_get, &pro_serial_ctrl_put,
	                     &pro_serial_reset, &pro_serial_exit};

int			pro_lp_workaround = 0;				/* workaround for Linux /dev/lp driver */
char			*pro_serial_devname[4];				/* serial device names */

LOCAL	unsigned char	pro_ser_rbuf[PRO_SER_NUMDEV][PRO_SER_RSIZE];	/* read buffer */
LOCAL	int		pro_ser_level[PRO_SER_NUMDEV] = {0, 0, 0};	/* fullness of read buffer */
LOCAL	int		pro_ser_xfer[PRO_SER_NUMDEV] = {0, 0, 0};	/* amount read */
LOCAL	int		pro_ser_cnt[PRO_SER_NUMDEV] = {0, 0, 0};

LOCAL	int		pro_ser_sfd[PRO_SER_NUMDEV] = {-1, -1, -1};

LOCAL	int		pro_ser_mlines_save[PRO_SER_NUMDEV];		/* modemlines to be restored on exit */
struct	termios		pro_ser_par_save[PRO_SER_NUMDEV];		/* parameters to be restored on exit */


/* Get character from serial port */

int pro_serial_get (int dev)
{
int	schar, sstat;


	if (pro_ser_cnt[dev]++ >= (PRO_SER_RSIZE-1))
	{
	  if (pro_ser_sfd[dev] != -1)
	  {
	    sstat = (int)read(pro_ser_sfd[dev], &pro_ser_rbuf[dev], PRO_SER_RSIZE);

	    if (sstat > 0)
	    {
	      pro_ser_xfer[dev] = sstat;
	      pro_ser_level[dev] = 0;
	    }
	  }

	  pro_ser_cnt[dev] = 0;
	}

	/* Return character if read buffer not empty */

	if (pro_ser_level[dev] == pro_ser_xfer[dev])
	  schar = PRO_NOCHAR;
	else
	  schar = (int)pro_ser_rbuf[dev][pro_ser_level[dev]++];

/* XXX
if (schar != PRO_NOCHAR)
  printf("R%d %d\r\n",dev, schar);
  printf("%c",c);
*/

	return schar;
}


/* Send character to serial port */

int pro_serial_put (int dev, int schar)
{
int		stat, bufdepth;
unsigned char	c;


	if (pro_ser_sfd[dev] != -1)
	{
	  c = (unsigned char)(schar);

	  /* Check number of characters in system write queue, and stall
	     if limit has been reached */

	  /* The system write buffer depth is currently limited to three characters.
	     Some serial devices don't support the ioctl needed to perform write
	     buffer occupancy checking.  If "lp_workaround" is turned on in the
	     config file, then the full depth of the host operating system's write
	     buffer is used.  (This is needed for Linux's /dev/lp, for example.)
	     This is also the case if ioctl support is not detected at compile-time.
	     (HPUX, for example)

	     There are serial port performance issues with P/OS if write buffer
	     occupancy checking is turned off. */

#ifdef TIOCOUTQ
	  if (pro_lp_workaround != 1)
	    ioctl(pro_ser_sfd[dev], TIOCOUTQ, &bufdepth);
	  else
#else
#warning TIOCOUTQ is not available.  Writes to serial ports not optimized!
#endif
	    bufdepth = 0;	/* lp_workaround: always allow a write to be attempted */

	  if (bufdepth > PRO_SERIAL_WDEPTH)
	    stat = PRO_FAIL;
	  else
	    switch (write(pro_ser_sfd[dev], &c, 1))
	    {
	      case -1:
	        stat = PRO_FAIL;
	        break;

	      default:
	        stat = PRO_SUCCESS;
	    }
	}
	else
	  stat = PRO_SUCCESS;

/* XXX
printf("W%d %d\r\n",dev, schar);
*/

	return stat;
}


/* Return serial line parameters */

void pro_serial_ctrl_get(int dev, struct serctrl *sctrl)
{
int	mlines;		/* modem lines */


	if (pro_ser_sfd[dev] != -1)
	{
	  ioctl(pro_ser_sfd[dev], TIOCMGET, &mlines);

	  sctrl->dsr = ((mlines&TIOCM_DSR) ? 1:0);
	  sctrl->cts = ((mlines&TIOCM_CTS) ? 1:0);
	  sctrl->ri = ((mlines&TIOCM_RI) ? 1:0);
	  sctrl->cd = ((mlines&TIOCM_CD) ? 1:0);

	  /* XXX unimplemented */

	  sctrl->perror = 0;
	  sctrl->ibrk = 0;
	}
}


/* Set serial line parameters */

void pro_serial_ctrl_put(int dev, struct serctrl *sctrl)
{
const tcflag_t cs[] = {CS5, CS6, CS7, CS8};

/* XXX 1.5 stop bits is implemented as 1 */

const tcflag_t stop[] = {0, 0, 0, CSTOPB};

const tcflag_t parity[] = {PARODD, 0};

const tcflag_t penable[] = {0, PARENB};

/* XXX 2000 and 3600 baud are implemented as 0 */
/* XXX 7200 baud is implemented as 115200 */

const speed_t baud[] = {B50, B75, B110, B134, B150, B300, B600, B1200,
	                B1800, B0, B2400, B0, B4800, B115200, B9600, B19200};

int	mlines;		/* modem lines */
struct	termios	serpar;	/* serial port parameters */


	if (pro_ser_sfd[dev] != -1)
	{
	  memset(&serpar, 0, sizeof(serpar));

	  mlines = 0;

	  serpar.c_iflag = IGNPAR | IGNBRK; /* XXX parity errors, break ignored */
	  serpar.c_oflag = 0;
	  serpar.c_cflag = CREAD | CLOCAL;
	  serpar.c_lflag = 0;

	  serpar.c_cflag |= cs[(sctrl->cs)&03];
	  serpar.c_cflag |= stop[(sctrl->stop)&03];
	  serpar.c_cflag |= parity[(sctrl->parity)&01];
	  serpar.c_cflag |= penable[(sctrl->penable)&01];

	  cfsetispeed(&serpar, baud[(sctrl->ibaud)&017]);
	  cfsetospeed(&serpar, baud[(sctrl->obaud)&017]);

	  tcsetattr(pro_ser_sfd[dev], TCSANOW, &serpar);

	  /* Set modem control lines */

	  mlines |= ((sctrl->dtr) ? TIOCM_DTR:0);
	  mlines |= ((sctrl->rts) ? TIOCM_RTS:0);

	  ioctl(pro_ser_sfd[dev], TIOCMSET, &mlines);

	  /* Check if a break should be sent */

	  if ((sctrl->obrk) == 1)
	    tcsendbreak(pro_ser_sfd[dev], 0);
	}
}


/* Open serial port */

void pro_serial_reset (int dev, int portnum)
{
	if (pro_ser_sfd[dev] == -1)
	{
	  pro_ser_sfd[dev] = open(pro_serial_devname[portnum], O_RDWR|O_NONBLOCK);

	  if (pro_ser_sfd[dev] == -1)
	  {
	    perror("Serial port failure");
	  }
	  else
	  {
	    /* Save serial parameters for exit */

	    tcgetattr(pro_ser_sfd[dev], &pro_ser_par_save[dev]);
	    ioctl(pro_ser_sfd[dev], TIOCMGET, &pro_ser_mlines_save[dev]);
	  }
	}
}


/* Close serial port */

void pro_serial_exit (int dev)
{
	if (pro_ser_sfd[dev] != -1)
	{
	  /* Restore serial parameters */

	  tcsetattr(pro_ser_sfd[dev], TCSANOW, &pro_ser_par_save[dev]);
	  ioctl(pro_ser_sfd[dev], TIOCMSET, &pro_ser_mlines_save[dev]);

	  close(pro_ser_sfd[dev]);
	  pro_ser_sfd[dev] = -1;
	}
}
#endif
