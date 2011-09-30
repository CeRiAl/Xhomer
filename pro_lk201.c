/* pro_lk201.c: keyboard

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
		-MANY keyboard commands are ignored
		-only PF1..4 (ctrl-F1..F4) are implemented on keypad
*/

#ifdef PRO
#include "pdp11_defs.h"
#include "pro_lk201.h"

#define	PRO_LK201_FIFO_DEPTH	1024

/* Keyboard code entry points */

struct sercall pro_lk201 = {&pro_lk201_get, &pro_lk201_put,
	                    &pro_lk201_ctrl_get, &pro_lk201_ctrl_put,
	                    &pro_lk201_reset, &pro_lk201_exit};

LOCAL int	pro_lk201_shift;	/* Tracks whether shift key is depressed */
LOCAL int	pro_lk201_ctrl;		/* Tracks whether ctrl key is depressed */
LOCAL int	pro_lk201_locked;	/* Tracks whether keyboard is locked */
LOCAL int	pro_lk201_bell_dis;	/* Tracks whether bell is disabled */
LOCAL int	pro_lk201_click_dis;	/* Tracks whether keyclick is disabled */
LOCAL int	pro_lk201_param;	/* Tracks whether next received byte is command or parameter */
LOCAL int	pro_lk201_last_cmd;	/* Stores last command */

LOCAL int	pro_lk201_fifo_h;
LOCAL int	pro_lk201_fifo_t;
LOCAL int	pro_lk201_fifo[PRO_LK201_FIFO_DEPTH];


/* Put character in keyboard FIFO */

LOCAL void pro_lk201_fifo_put (int key)
{
	/* XXX First check if FIFO is full */

	pro_lk201_fifo[pro_lk201_fifo_h] = key;
	pro_lk201_fifo_h++;
	if (pro_lk201_fifo_h == PRO_LK201_FIFO_DEPTH)
	  pro_lk201_fifo_h = 0;
}


/* Get character from keyboard FIFO */

LOCAL int pro_lk201_fifo_get ()
{
	int key;
	
	/* Check if FIFO is empty */

	if (pro_lk201_fifo_h == pro_lk201_fifo_t)	
	  key = PRO_NOCHAR;
	else
	{
	  key = pro_lk201_fifo[pro_lk201_fifo_t];
	  pro_lk201_fifo_t++;
	  if (pro_lk201_fifo_t == PRO_LK201_FIFO_DEPTH)
	    pro_lk201_fifo_t = 0;
	}

	return key;
}


/* Get character from keyboard */

int pro_lk201_get (int dev)
{
int	key;

	if (pro_lk201_locked == 0)
	{
	  key = pro_lk201_fifo_get();

	  if (key == PRO_NOCHAR)
	  {
	    key = pro_keyboard_get();

	    if (key != PRO_NOCHAR)
	    {
	      if (key == PRO_LK201_SHIFT)
	        pro_lk201_shift = 1;
	      else if (key == PRO_LK201_CTRL)
	        pro_lk201_ctrl = 1;
	      else if (key == PRO_LK201_ALLUPS)
	      {
	        pro_lk201_shift = 0;
	        pro_lk201_ctrl = 0;
	      }
	      else if ((key == PRO_LK201_PERIOD) && (pro_lk201_shift == 1))
	        key = PRO_LK201_LT;
	      else if ((key == PRO_LK201_COMMA) && (pro_lk201_shift == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_LT);
	        pro_lk201_fifo_put(PRO_LK201_SHIFT);
	      }
	      else if ((key == PRO_LK201_PRINT) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_PF1);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_SETUP) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_PF2);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_FFOUR) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_PF3);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_BREAK) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_PF4);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_INT) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_F17);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_RESUME) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_F18);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_CANCEL) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_F19);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
	      else if ((key == PRO_LK201_MAIN) && (pro_lk201_ctrl == 1))
	      {
	        key = PRO_LK201_ALLUPS;
	        pro_lk201_fifo_put(PRO_LK201_F20);
	        pro_lk201_fifo_put(PRO_LK201_CTRL);
	      }
    
	    }
	  }

/* XXX
	  if (key != PRO_NOCHAR) printf("%d\r\n", key);
*/
	}
	else
	  key = PRO_NOCHAR;

	return key;
}


/* Send character to keyboard */

int pro_lk201_put (int dev, int key)
{
	if (pro_lk201_param == 0)
	{
	  pro_lk201_last_cmd = PRO_LK201_NOCMD;

	  switch(key)
	  {
	    case PRO_LK201_UNLCK:
	      pro_lk201_locked = 0;
	      break;

	    case PRO_LK201_LCK:
	      pro_lk201_locked = 1;

	      /* Acknowledge keyboard locked command */
  
	      pro_lk201_fifo_put(PRO_LK201_LCK_ACK);
	      break;

	    case PRO_LK201_CLICK_E:
	      pro_lk201_click_dis = 0;
	      pro_keyboard_click_on();
	      break;

	    case PRO_LK201_CLICK_D:
	      pro_lk201_click_dis = 1;
	      pro_keyboard_click_off();
	      break;

	    case PRO_LK201_BELL_D:
	      pro_lk201_bell_dis = 1;
	      break;

	    case PRO_LK201_BELL:
	      if (pro_lk201_bell_dis == 0)
	        pro_keyboard_bell();
	      break;

	    case PRO_LK201_AUTO_D:
	      pro_keyboard_auto_off();
	      break;

	    case PRO_LK201_AUTO_E:
	      pro_keyboard_auto_on();
	      break;

	    case PRO_LK201_ID:
	      /* Send firmware and hardware IDs */

	      pro_lk201_fifo_put(PRO_LK201_IDF);
	      pro_lk201_fifo_put(PRO_LK201_IDH);
	      break;

	    case PRO_LK201_PWRUP:
	      /* Send firmware and hardware IDs, error codes */

	      pro_lk201_fifo_put(PRO_LK201_IDF);
	      pro_lk201_fifo_put(PRO_LK201_IDH);
	      pro_lk201_fifo_put(PRO_LK201_NOERR);
	      pro_lk201_fifo_put(PRO_LK201_NOERR);
	      break;

	    case PRO_LK201_DECTOUCH:
	      /* XXX undocumented DECtouch query command */

	      pro_lk201_fifo_put(PRO_LK201_KB);
	      break;

	    default:
	      /* Save potential command for when its parameter is received */

	      pro_lk201_last_cmd = key;
	      break;
	  }
	}
	else
	{
	  switch(pro_lk201_last_cmd)
	  {
	    case PRO_LK201_BELL_E:
	      pro_lk201_bell_dis = 0;
	      pro_keyboard_bell_vol(key & PRO_LK201_VOL);
	      pro_lk201_last_cmd = PRO_LK201_NOCMD;
	      break;

	    default:
	      break;
	  }
	}

	/* Determine whether next byte will be a command or a parameter */

	if ((key & PRO_LK201_PARAMS) != 0)
	  pro_lk201_param = 0;
	else
	  pro_lk201_param = 1;

/* XXX
	printf("Keyboard received code %d\r\n", key);
*/

	return PRO_SUCCESS;
}


/* Return serial line parameters */

void pro_lk201_ctrl_get (int dev, struct serctrl *sctrl)
{
	/* DSR is hardwired high */

	sctrl->dsr = 1;
}


/* Set serial line parameters */

void pro_lk201_ctrl_put (int dev, struct serctrl *sctrl)
{
}


/* Reset keyboard */

void pro_lk201_reset (int dev, int portnum)
{
	pro_lk201_shift = 0;
	pro_lk201_ctrl = 0;
	pro_lk201_locked = 0;
	pro_lk201_bell_dis = 0;
	pro_lk201_click_dis = 0;
	pro_lk201_param = 0;
	pro_lk201_last_cmd = PRO_LK201_NOCMD;

	pro_lk201_fifo_h = 0;
	pro_lk201_fifo_t = 0;
}


/* Exit routine */

void pro_lk201_exit (int dev)
{
}
#endif
