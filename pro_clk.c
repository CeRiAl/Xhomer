/* pro_clk.c: real time clock

   Copyright (c) 1997-2004, Tarik Isani (xhomer@isani.org)

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
		-some bits should not be cleared by reset?
		-BCD mode not implemented
		-12 hour mode not implemented
		-interrupt throttle mode needs to be validated
		-alarm int could generate extra int, or none at all
*/

/* When enabled, generates interrupts at the following rates
   in "real" PRO time (or actual realtime if RTC is defined):

   RS	Frequency (Hz)
   --   --------------
    0   none
    1    256
    2    128
    3   8192
    4   4096
    5   2048
    6   1024
    7    512
    8    256
    9    128
   10     64
   11     32
   12     16
   13      8
   14      4
   15      2
*/

#ifdef PRO
#include "pdp11_defs.h"
#include "sim_defs.h" /* For sim_gtime() */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>


/* Data type for >32-bit time value, implemented in three
   32-bit integers.  This is used for tracking real time
   in 10's of uS.

   The range of mid and lo is restricted, to allow multiplication
   by 8192 (the highest timer frequency), with enough guard-band
   to detect overflows */
   
typedef struct
{
   int		hi;	/* 0-(2G-1) */
   int		mid;	/* 0-99,999 */
   int		lo;	/* 0-99,999 */
} bigtime;


int		pro_force_year = -1;	/* when > -1, will override host's RTC year value */
int		pro_int_throttle = 0;	/* when 1, will throttle timer interrupts */

LOCAL int	pro_clk_csr0;
LOCAL int	pro_clk_csr1;
LOCAL int	pro_clk_csr2;
LOCAL int	pro_clk_csr3;

LOCAL int	pro_clk_al_sec;		/* alarm registers */
LOCAL int	pro_clk_al_min;
LOCAL int	pro_clk_al_hour;

LOCAL int	pro_clk_rs;		/* freq. # for previous periodic int cycle */
LOCAL int	pro_clk_realtime;	/* indicates whether in realtime mode */
LOCAL int	pro_clk_numint;		/* number of outstanding interrupts */
LOCAL int	pro_clk_freq;		/* periodic int frequency in Hz */
LOCAL int	pro_clk_int_cnt;	/* interrupt counter used for update cycles */

LOCAL struct tm	*pro_time;

LOCAL int	pro_time_sec;
LOCAL int	pro_time_min;
LOCAL int	pro_time_hour;

/* Periodic interrupt frequencies (in Hz) */

LOCAL int	pro_clk_intf[16] = {8192, 256, 128, 8192, 4096, 2048, 1024, 512,
	                            256, 128, 64, 32, 16, 8, 4, 2};


#ifdef RTC
LOCAL bigtime 	pro_time_start, pro_time_current, pro_time_tmp;


/* Subtract two bigtime operands (op1-op2)

   It is assumed that both operands are positive and that op1
   is greater than or equal to op2 */

LOCAL bigtime bigtime_sub(bigtime op1, bigtime op2)
{
	bigtime		res;


	/* Subtract each segment individually, and carry any underflows */

	res.hi = op1.hi - op2.hi;

	if (op1.mid >= op2.mid)
	  res.mid = op1.mid - op2.mid;
	else
	{
	  res.mid = 100000 - op2.mid + op1.mid;
	  res.hi--;
	}

	if (op1.lo >= op2.lo)
	  res.lo = op1.lo - op2.lo;
	else
	{
	  res.lo = 100000 - op2.lo + op1.lo;

          if (res.mid > 0)
	    res.mid--;
          else
          {
            res.mid = 99999;
	    res.hi--;
          }
	}

	return res;
}


/* Add a bigtime operand to an int (op1+op2)

   op2 must be non-negative and may not exceed (2G-100,000) */

LOCAL bigtime bigtime_add(bigtime op1, int op2)
{
	int		tmp_res;
	bigtime		res;


	tmp_res = op1.lo + op2;

	res.lo = tmp_res % 100000;
	res.mid = ((tmp_res / 100000) + op1.mid) % 100000;
	res.hi = ((tmp_res / 100000) + op1.mid) / 100000 + op1.hi;

	return res;
}


/* Multiply a bigtime operand by an int (op1*op2)

   op2 must be non-negative and may not exceed 21,474 (2G/100,000) */

LOCAL bigtime bigtime_mul(bigtime op1, int op2)
{
	bigtime		res, tmp;


	tmp.hi = op1.hi * op2;
	tmp.mid = op1.mid * op2;
	tmp.lo = op1.lo * op2;

	res.lo = tmp.lo % 100000;
	res.mid = ((tmp.lo / 100000) + tmp.mid) % 100000;
	res.hi = ((tmp.lo / 100000) + tmp.mid) / 100000 + tmp.hi;

	return res;
}


/* Return time in 10's of microseconds */

LOCAL bigtime pro_time_get()
{
	struct timeval	tv;
	bigtime		res;


	gettimeofday(&tv, NULL);

	/* Calculate 10's of microseconds */

	res.lo = tv.tv_usec/10;

	/* Calculate seconds */

	res.mid = tv.tv_sec % 100000;
	res.hi = tv.tv_sec / 100000;

	return res;
}
#endif


/* Update pro_time variable */

LOCAL void pro_time_update()
{
	time_t	t;

	t = time(NULL);
	pro_time = localtime(&t);

	pro_time_sec = pro_time->tm_sec;
	pro_time_min = pro_time->tm_min;
	pro_time_hour = pro_time->tm_hour;

	/* Ignore leap seconds */

	if (pro_time_sec > 59)
	  pro_time_sec = 59;
}


/* Event scheduler */

LOCAL void pro_clk_sched ()
{
int interval, rs;

	/* Recalculate frequency and start time if RS has changed */

	rs = pro_clk_csr0 & PRO_CLK_RS;

	if (rs != pro_clk_rs)
	{
	  pro_clk_rs = rs;

	  /* Update pro_clk_freq */

	  pro_clk_freq = pro_clk_intf[pro_clk_rs];
#ifdef RTC
	  /* Initialize time register */

	  pro_time_start = bigtime_mul(pro_time_get(), pro_clk_freq);
#endif
	  /* Clear number of outstanding interrupts */

	  pro_clk_numint = 0;

	  /* Clear interrupt counter */

	  pro_clk_int_cnt = 0;
	}

	/* Determine delay interval until next interrupt */

	interval = PRO_EQ_CLK_IPS / pro_clk_freq;

#ifdef RTC
	/* Use shorter interval if more than 2 interrupts are outstanding.
	   This will allow the RTC to catch up. (Wait until the emulator
	   is in real-time mode - i.e. the ROM diagnostics have completed) */

	/* (this is only done when in interrupt throttle mode) */

	/* XXX This might cause a problem with emulated operating systems
           - needs to be validated */

	if (pro_int_throttle && pro_clk_realtime && (pro_clk_numint > 2))
	  interval = PRO_CLK_RTC_CATCHUP;
#endif
	/* Schedule event */

	pro_eq_sched(PRO_EVENT_CLK, interval);
}

/* Update-ended event scheduler */

LOCAL void pro_clk_uf_sched ()
{
	pro_eq_sched(PRO_EVENT_CLK_UF, PRO_EQ_CLK_UF);
}

/* Update-ended event handler */

void pro_clk_uf_eq ()
{
	/* Update time and date */
 
	pro_time_update();

	/* Clear update-in-progress bit */

	pro_clk_csr0 = pro_clk_csr0 & ~PRO_CLK_UIP;

	/* Set UF bit */

	pro_clk_csr2 = pro_clk_csr2 | PRO_CLK_UF;

	/* Trigger update-ended interrupt, if enabled */

	if ((pro_clk_csr1 & PRO_CLK_UIE) != 0)
	{
	  pro_clk_csr2 = pro_clk_csr2 | PRO_CLK_IRQF;
	  pro_int_set(PRO_INT_CLK);
	}

	/* Compare against alarm registers and trigger
	   alarm interrupt if appropriate */

	if (((pro_time_sec == pro_clk_al_sec)
	   || ((pro_clk_al_sec & PRO_CLK_AL_X) == PRO_CLK_AL_X))
	   && ((pro_time_min == pro_clk_al_min)
	   || ((pro_clk_al_min & PRO_CLK_AL_X) == PRO_CLK_AL_X))
	   && ((pro_time_hour == pro_clk_al_hour)
	   || ((pro_clk_al_hour & PRO_CLK_AL_X) == PRO_CLK_AL_X)))
	{
	  /* Set AF bit */

	  pro_clk_csr2 = pro_clk_csr2 | PRO_CLK_AF;

	  /* Trigger alarm interrupt, if enabled */

	  if ((pro_clk_csr1 & PRO_CLK_AIE) != 0)
	  {
	    pro_clk_csr2 = pro_clk_csr2 | PRO_CLK_IRQF;
	    pro_int_set(PRO_INT_CLK);
	  }
	}
}

/* Periodic interrupt event queue handler */

void pro_clk_eq ()
{
#ifdef RTC
int	set_int;
#endif
#ifdef LIMIT
unsigned long	num_us;		/* number uS to sleep for realtime operation */
#endif

	/* Set interrupt and schedule next interrupt, if enabled */

#ifdef RTC
	/* Check if enough instructions have been simulated
	   to switch to realtime mode */

	if (pro_clk_realtime == 0)
	{
	  set_int = 1;

	  if (sim_gtime() > PRO_CLK_RTC_SWITCH)
	    pro_clk_realtime = 1;
	}
	else
	{
	  /* Calculate number of outstanding interrupts, based on real time */

	  /* Get current time */

	  pro_time_current = bigtime_mul(pro_time_get(), pro_clk_freq);

	  /* Calculate number of outstanding interrupts (as 32-bit int) */

	  /* Perform the following: pro_clk_numint = (pro_time_current - pro_time_start)/100000; */

	  pro_time_tmp = bigtime_sub(pro_time_current, pro_time_start);

	  /* (There is an implicit division by 100000 here, as pro_time_tmp.lo is dropped) */

	  pro_clk_numint = pro_time_tmp.hi * 100000 + pro_time_tmp.mid;

	  /* Trigger int only if the previous one has been seen
	   * (and only if in interrupt throttle mode) */

	  /* This is required for interrupt throttle mode, but makes the
	   * emulator implementation slightly inaccurate.  If both the
	   * clock interrupt bit in the interrupt controller's interrupt
	   * request register and the periodic interrupt flag in the CSR
	   * are set, the OS is determined not to have seen the previous
	   * interrupt yet.  In this case, further interrupts are delayed.
	   * (Note that this is an experimental option!)
	   */

	  if (pro_int_throttle && ((pro_clk_csr2 & PRO_CLK_PF) != 0)
	     && ((pro_int_irr[(PRO_INT_CLK >> 8) & 0377] & (PRO_INT_CLK & 0377)) != 0))
	    set_int = 0;
	  else

	  /* Check number of outstanding interrupts */

#ifndef LIMIT
	  if (pro_clk_numint < 1)
	    set_int = 0;
	  else
#endif
	  {
#ifdef LIMIT
	    if (pro_clk_numint < 1)
	    {
	      /* Perform the following: num_us = (pro_time_current - pro_time_start)/pro_clk_freq; */

	      pro_time_tmp = bigtime_sub(pro_time_current, pro_time_start);

	      /* Since pro_clk_numint == 0, we are gauranteed that pro_time_tmp.hi and .mid are 0 */

	      num_us = (pro_time_tmp.lo * 10)/pro_clk_freq;
	      usleep(num_us);
	    }
#endif
	    set_int = 1;

	    /* Advance start time */

	    pro_time_start = bigtime_add(pro_time_start, 100000);
	  }
	}

	if (set_int == 1)
#endif
	{
	  /* Trigger interrupt only if frequency is not disabled */

	  if (pro_clk_rs != 0)
	  {
	    /* Trigger interrupt */

	    pro_clk_csr2 = pro_clk_csr2 | PRO_CLK_PF;

	    if ((pro_clk_csr1 & PRO_CLK_PIE) != 0)		/* if enabled */
	    {
	      pro_clk_csr2 = pro_clk_csr2 | PRO_CLK_IRQF;
	      pro_int_set(PRO_INT_CLK);
	    }
	  }

	  /* Advance interrupt counter and update time, if required */

	  pro_clk_int_cnt++;

	  if (pro_clk_int_cnt >= pro_clk_freq)
	  {
	    pro_clk_int_cnt = 0;

	    /* Inhibit update cycles if SET bit is set */

	    if ((pro_clk_csr1 & PRO_CLK_SET) == 0)
	    {
	      /* Set update-in-progress bit */

	      pro_clk_csr0 = pro_clk_csr0 | PRO_CLK_UIP;

	      /* Schedule update-ended event */

	      pro_clk_uf_sched ();
	    }
	  }
	}

	/* Schedule next interrupt */

	pro_clk_sched();
}

/* Clock registers */

int pro_clk_rd (int pa)
{
int	data;

	switch (pa & 017777776)
	{
	  case 017773000:
	    data = pro_time_sec; /* seconds */
	    break;

	  case 017773002:
	    data = pro_clk_al_sec; /* seconds alarm */
	    break;

	  case 017773004:
	    data = pro_time_min; /* minutes */
	    break;

	  case 017773006:
	    data = pro_clk_al_min; /* minutes alarm */
	    break;

	  case 017773010:
	    data = pro_time_hour; /* hours */
	    break;

	  case 017773012:
	    data = pro_clk_al_hour; /* hours alarm */
	    break;

	  case 017773014:
	    data = pro_time->tm_wday + 1; /* day */ /* XXX correct? */
	    break;

	  case 017773016:
	    data = pro_time->tm_mday; /* date */
	    break;

	  case 017773020:
	    data = pro_time->tm_mon + 1; /* month */
	    break;

	  case 017773022:
	    /* Venix is unhappy with (70 < year > 99).
	       The following workaround allows the year to
	       be forced from the configuration file. */

	    if (pro_force_year > -1)
	      data = pro_force_year;
	    else
	      data = pro_time->tm_year % 100; /* year */
	    break;

	  case 017773024:
	    data = pro_clk_csr0;
	    break;

	  case 017773026:
	    data = pro_clk_csr1;
	    break;

	  case 017773030:
	    data = pro_clk_csr2;

	    /* clear read-once bits */

	    pro_clk_csr2 = pro_clk_csr2 & ~(PRO_CLK_IRQF | PRO_CLK_PF
	                                   | PRO_CLK_AF | PRO_CLK_UF);

	    break;

	  case 017773032:
	    data = pro_clk_csr3;

	    /* set read-once bit */

	    pro_clk_csr3 = pro_clk_csr3 | PRO_CLK_VRT;

	    break;

	  default:
	    data = 0;
	    break;
	}

	return data;
}

void pro_clk_wr (int data, int pa, int access)
{
	switch (pa & 017777776)
	{
	  case 017773002:
	    WRITE_WB(pro_clk_al_sec, PRO_CLK_AL_SEC_W, access); /* seconds alarm */
	    break;

	  case 017773006:
	    WRITE_WB(pro_clk_al_min, PRO_CLK_AL_MIN_W, access); /* minutes alarm */
	    break;

	  case 017773012:
	    WRITE_WB(pro_clk_al_hour, PRO_CLK_AL_HOUR_W, access); /* hours alarm */
	    break;

	  case 017773000:
	  case 017773004:
	  case 017773010:
	  case 017773014:
	  case 017773016:
	  case 017773020:
	  case 017773022:

	    /* Writes to time registers are not supported */
/* XXX
	    printf("%10.0lf Realtime clock write\r\n", sim_gtime());
*/
	    break;

	  case 017773024:
	    WRITE_WB(pro_clk_csr0, PRO_CLK_CSR0_W, access);
	    break;

	  case 017773026:
	    WRITE_WB(pro_clk_csr1, PRO_CLK_CSR1_W, access);
	    break;

	  case 017773030:
	  case 017773032:
	    /* read-only */
	    break;

	  default:
	    break;
	}
}


/* Reset */

void pro_clk_reset ()
{
	pro_clk_csr0 = 0000;
	pro_clk_csr1 = PRO_CLK_DM | PRO_CLK_24H;
	pro_clk_csr2 = 0000;
	pro_clk_csr3 = PRO_CLK_VRT;	/* this indicates clock has not lost power */

	pro_clk_al_sec = 0;
	pro_clk_al_min = 0;
	pro_clk_al_hour = 0;

	pro_clk_realtime = 0;
	pro_clk_numint = 0;
	pro_clk_freq = 0;
	pro_clk_int_cnt = 0;

#ifdef RTC
	pro_time_start.hi = 0;
	pro_time_start.mid = 0;
	pro_time_start.lo = 0;

	pro_time_current.hi = 0;
	pro_time_current.mid = 0;
	pro_time_current.lo = 0;
#endif

	/* Initialize time */

	pro_time_update();

	/* Force initialization of pro_clk_freq and pro_time_start */

	pro_clk_rs = -1;

	/* Schedule first periodic event */

	pro_clk_sched();
}
#endif
