/* pro_init.c: reset and exit routines

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


#ifdef PRO
#include "pdp11_defs.h"
#include <stdlib.h>
#include <unistd.h>

#ifdef __hpux__
#define seteuid(euid) setresuid(-1,(euid),-1)
#endif

extern t_stat ttclose (void); /* from scp_tty.c */
extern int PC; /* from pdp11_cpu */

#ifdef IOTRACE
FILE	*iotfptr = NULL;
#endif

struct	sercall	*pro_la50device;
struct	sercall	*pro_kb;
struct	sercall	*pro_ptr;
struct	sercall	*pro_com;

int	pro_la50device_port;
int	pro_kb_port;
int	pro_ptr_port;
int	pro_com_port;

int	pro_exit_on_halt = 1;

#ifdef TRACE
/* PC Trace */

LOCAL FILE		*tfptr = NULL;

int trace ()
{
        if (tfptr)
	{
	  fprintf(tfptr, "%o\n", PC);
	  fflush(tfptr);
	}
	return SCPE_OK;
}

LOCAL int trace_reset ()
{
        if (!tfptr)
	  tfptr = fopen("trace","w");
        else
          fflush(tfptr);

	return SCPE_OK;
}
#endif

#ifdef IOTRACE
/* I/O Access Trace */

LOCAL int iotrace_reset ()
{
        if (!iotfptr)
	  iotfptr = fopen("iotrace","w");
	else
	  fflush(iotfptr);

	return SCPE_OK;
}
#endif


/* Text parameter comparison */

/* max = -1 for no suffix
          0..9 for suffix */

/* returns -1 if no match
            0 if a string without a suffix matches (i.e. "kb")
            0..max if a string with a suffix matches (i.e. "rx2") */

LOCAL int scmp(char *s1, char *s2, int max, int p1, int p2, int numpar)
{
int	i, s1len, s2len;


	s1len = strlen(s1);
	s2len = strlen(s2);

	if ((strncmp(s1, s2, s2len) == 0)
	    && ((numpar == p1) || (numpar == p2)))
	{
	  if ((max == -1) && (s1len == s2len))
	    return 0;
	  else if  (s1len == (s2len+1))
	  {
	    i = (int)s1[s2len] - 48;
	    if ((i>=0) && (i<=max))
	      return i;
	  }
	}
	return -1;
}


/* Serial device assignments */

/* returns 0 if successful, -1 if not */

LOCAL int assign_serial(char *s1, struct sercall **dev, int *port)
{
int	i, j;


	j = 0;

	if ((i = scmp(s1, "serial\0", 3, 1, 1, 1)) > -1)
	{
	  *dev = &pro_serial;
	  *port = i;
	}
	else if ((i = scmp(s1, "la50\0", -1, 1, 1, 1)) > -1)
	{
	  *dev = &pro_la50;
	  *port = 0;
	}
	else if (scmp(s1, "lk201\0", -1, 1, 1, 1) > -1)
	{
	  *dev = &pro_lk201;
	  *port = 0;
	}
	else if (scmp(s1, "digipad\0", -1, 1, 1, 1) > -1)
	{
	  *dev = &pro_digipad;
	  *port = 0;
	}
	else if (scmp(s1, "null\0", -1, 1, 1, 1) > -1)
	{
	  *dev = &pro_null;
	  *port = 0;
	}
	else
	  j = -1;

	return j;
}


/* Main PRO reset routine */

void pro_reset ()
{
#define		MAXPAR	4	/* max # of parameters */

FILE		*fptr;
char		*eofptr;
char		str1[1024], str2[1024];
char		filename[1024];
char		*stra, *strv[MAXPAR];
unsigned char	h, l;
int		i, j, k, len, linenum, numpar, error, file_found;
char		*strtmp;


	printf("\n");
	printf("XHOMER: Digital Pro/350 emulator (version %s)\n", PRO_VERSION);
	printf("(Press ctrl-F1, while the emulator window has focus, to access the control menu)\n");
	printf("\n");

	seteuid(getuid());

	pro_csr = PRO_CSR_INIT;

	pro_led = 0;


#ifndef BUILTIN_ROMS
	/* Load boot/diagnostic ROM */

	fptr = fopen("pro350.rom","r");

	if (fptr == NULL)
	{
	  printf("Unable to open pro350.rom\n");
	  exit(0);
	}

	for(i=0 ; i<8192; i++)
	{
	  l = getc(fptr);
	  h = getc(fptr);
	  ROM[i] = h*256+l;
	}
	  
	fclose(fptr);


	/* Load ID ROM */

	fptr = fopen("id.rom","r");

	if (fptr == NULL)
	{
	  printf("Unable to open id.rom\n");
	  exit(0);
	}

	for(i=0 ; i<32; i++)
	{
	  l = getc(fptr);
	  h = getc(fptr);
	  IDROM[i] = h*256+l;
	}
	  
	fclose(fptr);
#endif


	/* Initialize battery-backed RAM */

	fptr = fopen("bat.ram","r");

	if (fptr == NULL)
	{
	  printf("Unable to open bat.ram - clearing battery-backed RAM\n");
	  memset(&BATRAM, 0, sizeof(BATRAM));
	}
	else
	{
	  for(i=0; i<50; i++)
	  {
	    l = getc(fptr);
	    h = getc(fptr);
	    BATRAM[i] = h*256+l;
	  }

	  fclose(fptr);
	}


#if PRO_MEM_PRESENT
	/* Initialize memory expansion board ROM */

	fptr = fopen("mem.rom","r");

	if (fptr == NULL)
	{
	  printf("Unable to open mem.rom\n");
	  exit(0);
	}

	for(i=0; i<1024; i++)
	{
	  l = getc(fptr);
	  MEMROM[i] = l;
	}

	fclose(fptr);
#endif


	/* Make default serial port assignments */

	pro_la50device = &pro_null;
	pro_la50device_port = 0;

	pro_kb = &pro_lk201;
	pro_kb_port = 0;

	pro_ptr = &pro_null;
	pro_ptr_port = 0;

	pro_com = &pro_null;
	pro_com_port = 0;


	/* Make default rd assignments */

	if (pro_rd_file != NULL)
	  free(pro_rd_file);
	pro_rd_file = strdup("pos32.rd");

	pro_rd_heads = 0;
	pro_rd_cyls = 0;
	pro_rd_secs = 0;

	/* Make default rx assignments */

	for(k=0; k<4; k++)
	{
	  if (pro_rx_file[k] != NULL)
	    free(pro_rx_file[k]);
	  pro_rx_file[k] = strdup("");
	}

	/* Make default disk image directory assignments */

	for(k=0; k<4; k++)
	{
	  if (pro_rx_dir[k] != NULL)
	    free(pro_rx_dir[k]);
	  pro_rx_dir[k] = strdup("./");
	}

	if (pro_rd_dir != NULL)
	  free(pro_rd_dir);
	pro_rd_dir = strdup("./");

	/* Read configuration file */

	file_found = 0;

	/* First try home directory */

	/* If $HOME is not defined, then look for /.xhomerrc */

	sprintf(filename, "%.1000s/%s", getenv("HOME")?getenv("HOME"):"", ".xhomerrc");
	fptr = fopen(filename,"r");

	if (fptr != NULL)
	  file_found = 1;
	else
	{
	  printf("%s not found\n", filename);

	  /* Get current working directory.  If path exceeds ~1000 characters,
	     simply use "./" */

	  if (getcwd(str1, 1000) == NULL)
	    sprintf(str1, ".");

	  sprintf(filename, "%s/%s", str1, "xhomer.cfg");
	  fptr = fopen(filename,"r");

	  if (fptr != NULL)
	    file_found = 1;
	  else
	    printf("%s not found\n", filename);
	}

	if (!file_found)
	{
	  printf("Unable to open ~/.xhomerrc or xhomer.cfg - using built-in defaults\n");
	}
	else
	{
	  printf("Using %s\n", filename);

	  linenum = 0;
	  do
	  {
	    linenum++;
	    eofptr = fgets(str1, 1024, fptr);
	    if (eofptr != 0)
	    {
	      /* Remove control characters, spaces and comments */

	      j = 0;

	      for(i=0; i<=strlen(str1); i++)
	        if (str1[i] == '#')
	        {
	          str2[j++] = 0;
	          break;
	        }
	        else if ((str1[i] > 32) || (str1[i] == 0))
	          str2[j++] = str1[i];

	      /* Filter blank lines */

	      if (strlen(str2) != 0)
	      {
	        /* Extract argument */

	        /* XXX should these be freed? */

	        stra = strtok(str2, "=");

	        /* Extract up to four parameters */

	        numpar = 0; /* # of parameters found */

	        do
	        {
	          strv[numpar] = strtok(NULL, ",");
	          if (strv[numpar] == NULL)
	            break;
	          numpar++;
	        }
	        while (numpar < MAXPAR);

	        /* Make assignments */

	        error = 0;

	        /* RD hard drive */

	        if ((i = scmp(stra, "rd", 0, 1, 4, numpar)) > -1)
	        {
	          if (pro_rd_file != NULL)
	            free(pro_rd_file);
	          pro_rd_file = strdup(strv[0]);

	          /* Check if geometry is defined */

	          if (numpar == 4)
	          {
	            pro_rd_heads = atoi(strv[1]);
	            pro_rd_cyls = atoi(strv[2]);
	            pro_rd_secs = atoi(strv[3]);
	          }
	        }

	        /* RX floppy */

	        else if ((i = scmp(stra, "rx", 3, 1, 1, numpar)) > -1)
	        {
	          if (pro_rx_file[i] != NULL)
	            free(pro_rx_file[i]);
	          pro_rx_file[i] = strdup(strv[0]);

	          /* Close drive */

	          pro_rx_closed[i] = 1;
	        }

	        /* RD image directory */

	        else if (scmp(stra, "rd_dir", -1, 1, 1, numpar) > -1)
	        {
	          if (pro_rd_dir != NULL)
	            free(pro_rd_dir);
	          pro_rd_dir = strdup(strv[0]);

	          len = strlen(pro_rd_dir);

	          if (pro_rd_dir[len-1] != '/')
	          {
	            strtmp = strdup(pro_rd_dir);
	            free(pro_rd_dir);
	            pro_rd_dir = malloc(len+2);
	            strcpy(pro_rd_dir, strtmp);
	            strcat(pro_rd_dir, "/");
	            free(strtmp);
	          }
	        }

	        /* RX image directory */

	        else if (scmp(stra, "rx_dir", -1, 1, 1, numpar) > -1)
	        {
	          for(k=0; k<4; k++)
	          {
	            if (pro_rx_dir[k] != NULL)
	              free(pro_rx_dir[k]);
	            pro_rx_dir[k] = strdup(strv[0]);

	            len = strlen(pro_rx_dir[k]);

	            if (pro_rx_dir[k][len-1] != '/')
	            {
	              strtmp = strdup(pro_rx_dir[k]);
	              free(pro_rx_dir[k]);
	              pro_rx_dir[k] = malloc(len+2);
	              strcpy(pro_rx_dir[k], strtmp);
	              strcat(pro_rx_dir[k], "/");
	              free(strtmp);
	            }
	          }
	        }

	        /* Serial device names */

	        else if ((i = scmp(stra, "serial", 3, 1, 1, numpar)) > -1)
	        {
	          if (pro_serial_devname[i] != NULL)
	            free(pro_serial_devname[i]);
	          pro_serial_devname[i] = strdup(strv[0]);
	        }

	        /* LA50 device */

	        else if (scmp(stra, "la50", -1, 1, 1, numpar) > -1)
	        {
	          if (assign_serial(strv[0], &pro_la50device, &pro_la50device_port) == -1)
	            error = 1;
	        }

	        /* Keyboard port */

	        else if (scmp(stra, "kb", -1, 1, 1, numpar) > -1)
	        {
	          if (assign_serial(strv[0], &pro_kb, &pro_kb_port) == -1)
	            error = 1;
	        }

	        /* Printer port */

	        else if (scmp(stra, "ptr", -1, 1, 1, numpar) > -1)
	        {
	          if (assign_serial(strv[0], &pro_ptr, &pro_ptr_port) == -1)
	            error = 1;
	        }

	        /* Communications port */

	        else if (scmp(stra, "com", -1, 1, 1, numpar) > -1)
	        {
	          if (assign_serial(strv[0], &pro_com, &pro_com_port) == -1)
	            error = 1;
	        }

	        /* Number of DGA framebuffers */

	        else if (scmp(stra, "framebuffers", -1, 1, 1, numpar) > -1)
	        {
	          i = atoi(strv[0]);
	          if ((i < 0) || (i > 3))
	            error = 1;
	          else
	            pro_screen_framebuffers = i;
	        }

	        /* Screen mode */

	        else if (scmp(stra, "screen", -1, 1, 1, numpar) > -1)
	        {
	          if (scmp(strv[0], "window", -1, 1, 1, 1) > -1)
	            pro_screen_full = 0;
	          else if (scmp(strv[0], "full", -1, 1, 1, 1) > -1)
	            pro_screen_full = 1;
	          else
	            error = 1;
	        }

	        /* Screen scale factors */

	        else if (scmp(stra, "window_scale", -1, 1, 1, numpar) > -1)
	        {
	          i = atoi(strv[0]);
	          if (i < 1)
	            error = 1;
	          else
	            pro_screen_window_scale = i;
	        }

	        else if (scmp(stra, "full_scale", -1, 1, 1, numpar) > -1)
	        {
	          i = atoi(strv[0]);
	          if (i < 1)
	            error = 1;
	          else
	            pro_screen_full_scale = i;
	        }

	        /* Window position */

	        else if (scmp(stra, "window_position", -1, 2, 2, numpar) > -1)
	        {
	          pro_window_x = atoi(strv[0]);
	          pro_window_y = atoi(strv[1]);
	        }

	        /* Screen gamma */

	        else if (scmp(stra, "screen_gamma", -1, 1, 1, numpar) > -1)
	        {
	          pro_screen_gamma = atoi(strv[0]);
	        }

	        /* la50 DPI */

	        else if (scmp(stra, "la50_dpi", -1, 1, 1, numpar) > -1)
	        {
	          i = atoi(strv[0]);

	          if ((i != 300) && (i != 600))
	            error = 1;
	          else
	            pro_la50_dpi = i;
	        }

	        /* Force year */

	        else if (scmp(stra, "force_year", -1, 1, 1, numpar) > -1)
	        {
	          i = atoi(strv[0]);

	          if ((i < -1) || (i > 99))
	            error = 1;
	          else
	            pro_force_year = i;
	        }

		/* Printer port maintenance mode */

		else if (scmp(stra, "maint_mode", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_maint_mode = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_maint_mode = 1;
	          else
	            error = 1;
		}

		/* Periodic interrupt throttle mode */

		else if (scmp(stra, "int_throttle", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_int_throttle = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_int_throttle = 1;
	          else
	            error = 1;
		}

		else if (scmp(stra, "nine_workaround", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_nine_workaround = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_nine_workaround = 1;
	          else
	            error = 1;
		}

		else if (scmp(stra, "libc_workaround", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_libc_workaround = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_libc_workaround = 1;
	          else
	            error = 1;
		}

		else if (scmp(stra, "lp_workaround", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_lp_workaround = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_lp_workaround = 1;
	          else
	            error = 1;
		}

		else if (scmp(stra, "pcm", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_screen_pcm = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_screen_pcm = 1;
	          else
	            error = 1;
		}

		else if (scmp(stra, "exit_on_halt", -1, 1, 1, numpar) > -1)
		{
	          if (scmp(strv[0], "off", -1, 1, 1, 1) > -1)
	            pro_exit_on_halt = 0;
	          else if (scmp(strv[0], "on", -1, 1, 1, 1) > -1)
	            pro_exit_on_halt = 1;
	          else
	            error = 1;
		}

	        else
	          error = 1;

	        if (error == 1)
	          printf("Config file error line %d: %s", linenum, str1);
	      }
	    }
	  }
	  while (eofptr != 0);
	  fclose(fptr);
	}


	pro_eq_reset();

	pro_int_reset();

	pro_clk_reset();

	pro_mem_reset();

	pro_kb_reset();

	pro_ptr_reset();

	pro_com_reset();

	pro_rx_reset();

	pro_rd_reset();

	pro_menu_reset();

	pro_vid_reset();

#ifdef TRACE
	trace_reset();
#endif

#ifdef IOTRACE
	iotrace_reset();
#endif
}


/* Exit routine */

void pro_exit ()
{
FILE		*fptr;
unsigned char	h, l;
int		i;


	/* Write battery-backed RAM data back to file */

	fptr = fopen("bat.ram","w");

	if (fptr == NULL)
	{
	  printf("Unable to write to bat.ram\n");
	}
	else
	{
	  for(i=0; i<50; i++)
	  {
	    l = BATRAM[i] & 0xff;
	    h = (BATRAM[i] >> 8) & 0xff;
	    putc(l, fptr);
	    putc(h, fptr);
	  }

	  fclose(fptr);
	}

	pro_mem_exit();

	pro_rx_exit();

	pro_rd_exit();

	pro_kb_exit();

	pro_ptr_exit();

	pro_com_exit();

	pro_vid_exit();

	/* Call scp_tty to restore terminal settings */

	ttclose();

	exit(0);
}
#endif
