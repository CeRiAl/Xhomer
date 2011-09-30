/* pro_menu.c: Menu subsystem

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
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pdp11_defs.h"
#include "pro_lk201.h"

#define MENU_X		24			/* x position of top of menu */
#define MENU_Y		2			/* y position of top of menu */

#define MENU_BW		4			/* width of menu border */
#define MENU_AW		30			/* active width of menu */
#define MENU_W		2*MENU_BW+MENU_AW	/* width of menu */

#define MENU_BH		2			/* height of menu border */
#define MENU_AH		MENU_TOP_NUM		/* active height of menu */
#define MENU_H		2*MENU_BH+2+MENU_AH	/* height of menu */

#define MENU_PX		(MENU_X+1)
#define MENU_PY		(MENU_Y+MENU_BH+2+pro_menu_pos)

#define MENU_TOP_NUM	13			/* number of top level menu items */
#define MENU_SERIAL_NUM	8			/* number of serial menu items */
#define MENU_ABOUT_NUM	13			/* number of about menu items */

#define MENU_TOP	0
#define MENU_SERIAL	1
#define MENU_FILE	2
#define MENU_ABOUT	3

#define MENU_TOP_RETURN	0
#define MENU_TOP_SCREEN	1
#define MENU_TOP_RX0	2
#define MENU_TOP_RX1	3
#define MENU_TOP_RD0	4
#define MENU_TOP_COM	5
#define MENU_TOP_PTR	6
#define MENU_TOP_KB	7
#define MENU_TOP_SAVE	8
#define MENU_TOP_REST	9
#define MENU_TOP_RESET	10
#define MENU_TOP_ABOUT	11
#define MENU_TOP_SHUT	12

#define MENU_MAXENTRIES	1024		/* maximum number of menu entries */
#define MENU_MAXFILELEN	255		/* maximum filename length */

LOCAL int	pro_menu_last_char;	/* tracks last char, for auto-repeat */
LOCAL int	pro_menu_key_ctrl;	/* tracks whether ctrl key is depressed */
LOCAL int	pro_menu_on;		/* tracks whether menu is on */
LOCAL int	pro_menu_top_start;	/* start item number of item list */
LOCAL int	pro_menu_top_pos;	/* menu item selected */
LOCAL int	pro_menu_serial_start;	/* start item number of item list */
LOCAL int	pro_menu_serial_pos;	/* position of serial menu bar */
LOCAL int	pro_menu_file_start;	/* start item number of item list */
LOCAL int	pro_menu_file_pos;	/* position of file menu bar */
LOCAL int	pro_menu_active;	/* currently active menu */

LOCAL int	pro_menu_start;		/* start postion */
LOCAL int	pro_menu_pos;		/* current position */
LOCAL int	pro_menu_items;		/* number of menu items */

LOCAL char	title[MENU_AW+1];
LOCAL char	blank[MENU_W+1];
LOCAL char	blank_top[MENU_W+1];
LOCAL char	blank_bot[MENU_W+1];
LOCAL char	item[MENU_MAXENTRIES][MENU_MAXFILELEN+1];
LOCAL int	item_ok[MENU_MAXENTRIES];
LOCAL char	menu_bar[MENU_W+1];


/* Place menu bar on menu */

void pro_menu_bar (int pro_menu_pos)
{
	pro_overlay_print(MENU_PX, MENU_PY, 1, 1, menu_bar);
}


/* Concatenate s2 and s3 into s1, limited by length n */

void pro_menu_concat (char *s1, char *s2, char *s3, int n)
{
int	i, pos;


	pos = 0;

	if (s2 != NULL)
	  for(i=0; i<strlen(s2); i++)
	  if (pos<n)
	    s1[pos++] = s2[i];

	if (s3 != NULL)
	  for(i=0; i<strlen(s3); i++)
	  if (pos<n)
	    s1[pos++] = s3[i];

	s1[pos] = '\0';
}


/* Make serial assignments */

void pro_menu_assign_serial (int devnum, struct sercall **dev, int *port)
{
	switch(devnum)
	{
	  case 0:
	  case 1:
	  case 2:
	  case 3:
	    *dev = &pro_serial;
	    *port = devnum;
	    break;

	  case 4:
	    *dev = &pro_digipad;
	    *port = 0;
	    break;

	  case 5:
	    *dev = &pro_lk201;
	    *port = 0;
	    break;

	  case 6:
	    *dev = &pro_la50;
	    *port = 0;
	    break;

	  case 7:
	    *dev = &pro_null;
	    *port = 0;
	    break;

	  default:
	    break;
	}
}


/* Return s2 + serial assignment printout in s1 */
/* Also return menu item number */

int pro_menu_print_serial (char *s1, char *s2, struct sercall *dev, int port)
{
int	item;
char	s3[MENU_AW+1], s4[MENU_AW+1];


	item = 0;

	if (dev == &pro_serial)
	{
	  sprintf(s4, "serial%d (", port);
	  pro_menu_concat(s3, s4, pro_serial_devname[port], MENU_AW);
	  pro_menu_concat(s4, s3, ")", MENU_AW);
	  item = port;
	}
	else if (dev == &pro_digipad)
	{
	  sprintf(s4, "digipad");
	  item = 4;
	}
	else if (dev == &pro_lk201)
	{
	  sprintf(s4, "lk201");
	  item = 5;
	}
	else if (dev == &pro_la50)
	{
	  sprintf(s4, "la50");
	  item = 6;
	}
	else if (dev == &pro_null)
	{
	  sprintf(s4, "null");
	  item = 7;
	}

	pro_menu_concat(s1, s2, s4, MENU_AW);

	return item;
}


/* Update directory path name */

void pro_menu_newdir(char **dir, char *new)
{
int	i, len;
char	*temp;


	/* Do nothing if current directory is specified */

	if (strcmp(new, ".") != 0)
	{
	  temp = strdup(*dir);

	  if (*dir != NULL)
	    free(*dir);

	  if (strcmp(new, "..") == 0)
	  {
	    /* Up one level */

	    /* Get last directory name
	       Remove it if it is not . or ..
	       Otherwise, add on a .. */

	    len = strlen(temp);

	    for(i=len-1; i>0; i--)
	    {
	      if (temp[i-1] == '/')
	        break;
	    }

	    if ((strcmp(temp+i, "./") == 0) || (strcmp(temp+i, "../") == 0))
	    {
	      *dir = malloc(len + 4);
	      strcpy(*dir, temp);
	      strcat(*dir, "../");
	    }
	    else
	    {
	      temp[i] = '\0';
	      *dir = strdup(temp);
	    }
	  }
	  else
	  {
	    /* Down one level */

	    len = strlen(temp) + strlen(new) + 2;
	    *dir = malloc(len);
	    strcpy(*dir, temp);
	    strcat(*dir, new);
	    strcat(*dir, "/");
	  }

	  free(temp);
	}
}


/* Print menu */

void pro_menu_print (int start)
{
int	i;
char	itmp[MENU_AW+1];


	blank_top[0] = 24;
	blank_top[MENU_W-1] = 26;

	blank[0] = 31;
	blank[MENU_W-1] = 27;

	blank_bot[0] = 30;
	blank_bot[MENU_W-1] = 28;

	for(i=1; i<MENU_W-1; i++)
	{
	  blank_top[i] = 25;
	  blank[i] = 32;
	  blank_bot[i] = 29;
	}

	blank_top[MENU_W] = '\0';
	blank[MENU_W] = '\0';
	blank_bot[MENU_W] = '\0';

	pro_overlay_print(MENU_X, MENU_Y, 0, 1, blank_top);

	for(i=1; i<MENU_H-1; i++)
	  pro_overlay_print(MENU_X, MENU_Y+i, 0, 1, blank);

	pro_overlay_print(MENU_X, MENU_Y+MENU_H-1, 0, 1, blank_bot);

	pro_overlay_print(MENU_X+MENU_BW, MENU_Y+MENU_BH, 0, 1, title);

	for(i=0; i<MENU_AH; i++)
	  if ((i+start)<pro_menu_items)
	  {
	    pro_menu_concat(itmp, "", item[i+start], MENU_AW);
	    pro_overlay_print(MENU_X+MENU_BW, MENU_Y+MENU_BH+2+i, 0, item_ok[i+start], itmp);
	  }
}


/* Initialize and print file menu */

void pro_menu_file (char **filename)
{
DIR		*dp;
struct dirent	*ep;
struct stat	statbuf;
int		i, j;
char		fname[MENU_MAXFILELEN+1];
char		sname[MENU_MAXFILELEN+1];
char		strtemp[MENU_MAXFILELEN+1];
char		*dir = NULL;


	pro_menu_active = MENU_FILE;
	pro_menu_items = 1;
	pro_menu_file_pos = 0;

	if (filename == &pro_rx_file[0])
	{
	  sprintf(strtemp, "Floppy 0 : ");
	  dir = pro_rx_dir[0];
	}
	else if (filename == &pro_rx_file[1])
	{
	  sprintf(strtemp, "Floppy 1 : ");
	  dir = pro_rx_dir[1];
	}
	else if (filename == &pro_rd_file)
	{
	  sprintf(strtemp, "Hard disk 0 : ");
	  dir = pro_rd_dir;
	}

	sprintf(item[0], "  ");

	pro_menu_concat(title, strtemp, dir, MENU_AW);

	dp = opendir(dir);
	if (dp != NULL)
	{
	  do
	  {
	    ep = readdir(dp);

	    if (ep != NULL)
	    {
	      pro_menu_concat(fname, "", ep->d_name, MENU_MAXFILELEN);
	      pro_menu_concat(sname, dir, ep->d_name, MENU_MAXFILELEN);
	      stat(sname, &statbuf);

	      /* Check if link or regular file or directory */

	      if ((statbuf.st_mode&S_IFMT) == S_IFDIR)
	      {
	        pro_menu_concat(item[pro_menu_items], "1 ", fname, MENU_MAXFILELEN);
	        item_ok[pro_menu_items] = 1;
	        pro_menu_items++;
	      }
	      else if ((statbuf.st_mode&S_IFMT) == S_IFLNK)
	      {
	        pro_menu_concat(item[pro_menu_items], "2 ", fname, MENU_MAXFILELEN);
	        item_ok[pro_menu_items] = 1;
	        pro_menu_items++;
	      }
	      else if ((statbuf.st_mode&S_IFMT) == S_IFREG)
	      {
	        pro_menu_concat(item[pro_menu_items], "3 ", fname, MENU_MAXFILELEN);
	        item_ok[pro_menu_items] = 1;
	        pro_menu_items++;
	      }
	    }
	  }
	  while ((ep != NULL) && (pro_menu_items < (MENU_MAXENTRIES-1)));

	  (void) closedir(dp);

	  /* Sort directory */

	  for(j=pro_menu_items-1; j>1; j--)
	    for(i=pro_menu_items-1; i>1; i--)
	    {
	      if (strcmp(item[i], item[i-1]) < 0)
	      {
	        /* swap */

	        strcpy(strtemp, item[i-1]);
	        strcpy(item[i-1], item[i]);
	        strcpy(item[i], strtemp);
	      }
	    }

	  /* Replace first character and search for current name */

	  for(i=0; i<pro_menu_items; i++)
	  {
	    if (strcmp(*filename, item[i]+2) == 0)
	      pro_menu_file_pos = i;

	    if (item[i][0] == '1')
	      item[i][0] = 'D';
	    else if (item[i][0] == '2')
	      item[i][0] = 'L';
	    else if (item[i][0] == '3')
	      item[i][0] = ' ';
	  }
	}
	else
	  printf("Error: can't open directory\r\n");
     
	if (pro_menu_file_pos>MENU_AH-1)
	  pro_menu_file_start = pro_menu_file_pos - MENU_AH + 1;
	else
	  pro_menu_file_start = 0;

	pro_menu_pos = pro_menu_file_pos;
	pro_menu_start = pro_menu_file_start;

	pro_menu_print(pro_menu_start);
	pro_menu_bar(pro_menu_pos-pro_menu_start);
}


/* Initialize and print serial menu */

void pro_menu_serial (struct sercall **porttype, int port)
{
int	i;


	for(i=0; i<MENU_SERIAL_NUM; i++)
	  item_ok[i] = 1;

	pro_menu_serial_pos = pro_menu_print_serial(item[0], "", *porttype, port);
	if (pro_menu_serial_pos>(MENU_AH-1))
	  pro_menu_serial_start = pro_menu_serial_pos - MENU_AH + 1;
	else
	  pro_menu_serial_start = 0;

	/* Disable in-use ports from selection */

	if ((porttype != &pro_com) && (pro_com != &pro_null))
	  item_ok[pro_menu_print_serial(item[0], "", pro_com, pro_com_port)] = 0;

	if ((porttype != &pro_ptr) && (pro_ptr != &pro_null))
	  item_ok[pro_menu_print_serial(item[0], "", pro_ptr, pro_ptr_port)] = 0;

	if ((porttype != &pro_kb_orig) && (pro_kb_orig != &pro_null))
	  item_ok[pro_menu_print_serial(item[0], "", pro_kb_orig, pro_kb_port)] = 0;

	if ((porttype != &pro_la50device) && (pro_la50device != &pro_null))
	  item_ok[pro_menu_print_serial(item[0], "", pro_la50device, pro_la50device_port)] = 0;

	pro_menu_active = MENU_SERIAL;
	pro_menu_start = pro_menu_serial_start;
	pro_menu_pos = pro_menu_serial_pos;
	pro_menu_items = MENU_SERIAL_NUM;

	if (porttype == &pro_com)
	  sprintf(title, "Communications Port");
	else if (porttype == &pro_ptr)
	  sprintf(title, "Printer Port");
	else if (porttype == &pro_kb_orig)
	  sprintf(title, "Keyboard Port");

	pro_menu_print_serial(item[0], "", &pro_serial, 0);
	pro_menu_print_serial(item[1], "", &pro_serial, 1);
	pro_menu_print_serial(item[2], "", &pro_serial, 2);
	pro_menu_print_serial(item[3], "", &pro_serial, 3);
	pro_menu_print_serial(item[4], "", &pro_digipad, 0);
	pro_menu_print_serial(item[5], "", &pro_lk201, 0);
	pro_menu_print_serial(item[6], "", &pro_la50, 0);
	pro_menu_print_serial(item[7], "", &pro_null, 0);

	pro_menu_print(pro_menu_start);
	pro_menu_bar(pro_menu_pos-pro_menu_start);
}


/* Initialize and print "about XHOMER" page */

void pro_menu_about ()
{
int	i;


	for(i=0; i<MENU_ABOUT_NUM; i++)
	  item_ok[i] = 1;

	pro_menu_active = MENU_ABOUT;
	pro_menu_start = 0;
	pro_menu_pos = MENU_ABOUT_NUM - 1;
	pro_menu_items = MENU_ABOUT_NUM;

	sprintf(title, "About XHOMER");

	sprintf(item[0], "Welcome to  XHOMER, a  Digital");
	sprintf(item[1], "Pro/350   machine    emulator,");
	sprintf(item[2], "written by  Tarik Isani.  This");
	sprintf(item[3], "emulator   is   based   on   a");
	sprintf(item[4], "slightly  modified  version of");
	sprintf(item[5], "the  PDP-11 CPU core from  the");
	sprintf(item[6], "SIMH simulator.  Download from");
	sprintf(item[7], "http://xhomer.isani.org/");
	sprintf(item[8], " ");
	sprintf(item[9], "Please direct any questions or");
	sprintf(item[10], "feedback to: xhomer@isani.org");
	sprintf(item[11], " ");
	sprintf(item[12], "Previous menu");

	pro_menu_print(pro_menu_start);
	pro_menu_bar(pro_menu_pos-pro_menu_start);
}


/* Initialize and print top-level menu */

void pro_menu_top ()
{
int	i;

	for(i=0; i<MENU_AW+2*MENU_BW-2; i++)
	  menu_bar[i] = ' ';
	menu_bar[i] = '\0';

	for(i=0; i<MENU_TOP_NUM; i++)
	  item_ok[i] = 1;

	item_ok[MENU_TOP_RD0] = 0;
	item_ok[MENU_TOP_SAVE] = 0;
	item_ok[MENU_TOP_REST] = 0;
	item_ok[MENU_TOP_RESET] = 0;

	pro_menu_active = MENU_TOP;
	pro_menu_start = pro_menu_top_start;
	pro_menu_pos = pro_menu_top_pos;
	pro_menu_items = MENU_TOP_NUM;
	pro_menu_on = 1;
	pro_overlay_enable();

	sprintf(title, "XHOMER (version %s)", PRO_VERSION);
	sprintf(item[0],  "Return to emulator");
	sprintf(item[1], "Screen mode : %s", pro_screen_full?"full":"window");
	pro_menu_concat(item[2], "rx0 : ", pro_rx_file[0], MENU_AW);
	pro_menu_concat(item[3], "rx1 : ", pro_rx_file[1], MENU_AW);
	pro_menu_concat(item[4], "rd0 : ", pro_rd_file, MENU_AW);
	pro_menu_print_serial(item[5], "com : ", pro_com, pro_com_port);
	pro_menu_print_serial(item[6], "ptr : ", pro_ptr, pro_ptr_port);
	pro_menu_print_serial(item[7], "kb  : ", pro_kb_orig, pro_kb_port);
	sprintf(item[8], "Save configuration");
	sprintf(item[9], "Restore defaults and reset");
	sprintf(item[10], "Reset");
	sprintf(item[11], "About XHOMER");
	sprintf(item[12], "Shutdown emulator");

	pro_menu_print(pro_menu_start);
	pro_menu_bar(pro_menu_pos-pro_menu_start);
}


/* This routine filters keyboard characters
   Any characters which are not menu commands are passed through */

int pro_menu (int key)
{
int	retchar, schar, menu_pos_old;

	retchar = key;
	schar = key;

	/* Handle auto-repeat from real LK201 */

	if (schar == PRO_LK201_REPEAT)
	  schar = pro_menu_last_char;
	else
	  pro_menu_last_char = schar;

	if (schar == PRO_LK201_CTRL)
	  pro_menu_key_ctrl = 1;
	else if (schar == PRO_LK201_ALLUPS)
	  pro_menu_key_ctrl = 0;
	else if ((schar == PRO_LK201_DEL) && (pro_menu_key_ctrl == 1))
	  pro_exit();
	else if (pro_menu_on == 1)
	{
	  /* Process menu commands */

	  menu_pos_old = pro_menu_pos;

	  if (schar == PRO_LK201_UP)
	  {
	    /* Scroll menu bar up */

	    if (pro_menu_pos > 0)
	    {
	      pro_menu_bar(pro_menu_pos-pro_menu_start);

	      do
	        pro_menu_pos--;
	      while ((pro_menu_pos > 0) && (item_ok[pro_menu_pos] == 0));

	      if (item_ok[pro_menu_pos] == 0)
	        pro_menu_pos = menu_pos_old;

	      if (pro_menu_pos<pro_menu_start)
	      {
	        pro_menu_start = pro_menu_pos;
	        pro_menu_print(pro_menu_start);
	      }
	      pro_menu_bar(pro_menu_pos-pro_menu_start);
	    }
	    retchar = PRO_NOCHAR;
	  }
	  else if (schar == PRO_LK201_DOWN)
	  {
	    /* Scroll menu bar down */

	    if (pro_menu_pos < (pro_menu_items-1))
	    {
	      pro_menu_bar(pro_menu_pos-pro_menu_start);

	      do
	        pro_menu_pos++;
	      while ((pro_menu_pos < (pro_menu_items-1)) && (item_ok[pro_menu_pos] == 0));

	      if (item_ok[pro_menu_pos] == 0)
	        pro_menu_pos = menu_pos_old;

	      if (pro_menu_pos>pro_menu_start+MENU_AH-1)
	      {
	        pro_menu_start = pro_menu_pos - MENU_AH + 1;
	        pro_menu_print(pro_menu_start);
	      }
	      pro_menu_bar(pro_menu_pos-pro_menu_start);
	    }
	    retchar = PRO_NOCHAR;
	  }
	  else if ((schar == PRO_LK201_RETURN) || (schar == PRO_LK201_DO))
	  {
	    if (pro_menu_active == MENU_TOP)
	    {
	      /* Execute menu item */

	      pro_menu_top_start = pro_menu_start;
	      pro_menu_top_pos = pro_menu_pos;

	      switch (pro_menu_pos)
	      {
	        case MENU_TOP_RETURN:
	          /* Turn off menu */

	          pro_menu_on = 0;
	          pro_overlay_disable();
	          break;

	        case MENU_TOP_SCREEN:
	          /* Switch screen mode */

	          pro_screen_close();

	          if (pro_screen_full == 0)
	            pro_screen_full = 1;
	          else
	            pro_screen_full = 0;

	          if (pro_screen_init() == PRO_FAIL)
	          {
	            /* Switch mode back */

	            pro_screen_close();

	            if (pro_screen_full == 0)
	              pro_screen_full = 1;
	            else
	              pro_screen_full = 0;

	            pro_screen_init();
	          }

	          pro_menu_top();
	          break;

	        case MENU_TOP_RX0:
	        case MENU_TOP_RX1:
	          /* Floppy 0, 1 */

	          /* Open floppy door */

	          pro_rx_open_door(pro_menu_pos-MENU_TOP_RX0);

	          pro_menu_file(&pro_rx_file[pro_menu_pos-MENU_TOP_RX0]);
	          break;

	        case MENU_TOP_RD0:
	          /* Hard disk 0 */

	          pro_menu_file(&pro_rd_file);
	          break;

	        case MENU_TOP_COM:
	          /* com menu */

	          pro_menu_serial(&pro_com, pro_com_port);
	          break;

	        case MENU_TOP_PTR:
	          /* ptr menu */
  
	          pro_menu_serial(&pro_ptr, pro_ptr_port);
	          break;

	        case MENU_TOP_KB:
	          /* kb menu */
  
	          pro_menu_serial(&pro_kb_orig, pro_kb_port);
	          break;

		case MENU_TOP_ABOUT:
		  pro_menu_about();
		  break;

	        case MENU_TOP_SHUT:
	          pro_exit();
	          break;

	        default:
	          break;
	      }
	    }
	    else if (pro_menu_active == MENU_SERIAL)
	    {
	      switch (pro_menu_top_pos)
	      {
	        case MENU_TOP_COM:
	          pro_com_exit();
		  pro_menu_assign_serial(pro_menu_pos, &pro_com, &pro_com_port);
	          pro_com_open();
	          break;

	        case MENU_TOP_PTR:
	          pro_ptr_exit();
		  pro_menu_assign_serial(pro_menu_pos, &pro_ptr, &pro_ptr_port);
	          pro_ptr_open();
	          break;

	        case MENU_TOP_KB:
	          pro_kb_exit();
		  pro_menu_assign_serial(pro_menu_pos, &pro_kb, &pro_kb_port);
	          pro_kb_open();
	          break;

	        default:
	          break;
	      }

	      pro_menu_top();
	    }
	    else if (pro_menu_active == MENU_FILE)
	    {
	      switch (pro_menu_top_pos)
	      {
	        case MENU_TOP_RX0:
	        case MENU_TOP_RX1:
	          /* Check if directory, file or none */

	          if (item[pro_menu_pos][0] == 'D')
		  {
	            /* Update directory name */

	            pro_menu_newdir(&pro_rx_dir[pro_menu_top_pos-MENU_TOP_RX0], item[pro_menu_pos]+2);
	            pro_menu_file(&pro_rx_file[pro_menu_top_pos-MENU_TOP_RX0]);
	          }
	          else
	          {
	            /* Update file name */

	            if (pro_rx_file[pro_menu_top_pos-MENU_TOP_RX0] != NULL)
	              free(pro_rx_file[pro_menu_top_pos-MENU_TOP_RX0]);

	            pro_rx_file[pro_menu_top_pos-MENU_TOP_RX0] = strdup(item[pro_menu_pos]+2);

	            /* Close floppy door */

	            /* Check if no image */

	            if (pro_menu_pos != 0)
	              pro_rx_close_door(pro_menu_top_pos-MENU_TOP_RX0);
	            pro_menu_top();
	          }
	          break;

	        case MENU_TOP_RD0:
	          pro_menu_top();
	          break;

	        default:
	          break;
	      }
	    }
	    else if (pro_menu_active == MENU_ABOUT)
	    {
	      if (pro_menu_pos == (MENU_ABOUT_NUM-1))
	        pro_menu_top();
	    }

	    retchar = PRO_NOCHAR;
	  }
	}
	else if ((schar == PRO_LK201_PRINT) && (pro_menu_key_ctrl == 1))
	{
	  /* Turn on top-level menu */

	  pro_menu_top();

	  pro_menu_top_start = 0;
	  pro_menu_top_pos = 0;

	  retchar = PRO_NOCHAR;
	}

	return retchar;
}


/* Reset menu subsystem */

void pro_menu_reset ()
{
	pro_menu_last_char = 0;
	pro_menu_key_ctrl = 0;
	pro_menu_on = 0;
}
#endif
