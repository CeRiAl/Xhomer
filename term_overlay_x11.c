/* term_overlay_x11.c: X-windows overlay frame buffer code

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

#include <stdlib.h>
#include <string.h>
#include "pro_defs.h"
#include "pro_font.h"


int			pro_overlay_on = 0;
unsigned char		*pro_overlay_data;	/* overlay frame buffer */
unsigned char		*pro_overlay_alpha;	/* overlay frame buffer alpha */

LOCAL int		pro_overlay_open = 0;
LOCAL int		clutmode;
LOCAL int		pixsize;
LOCAL int		blackpixel;
LOCAL int		whitepixel;
LOCAL int		overlay_a;
LOCAL int		start_x;
LOCAL int		last_x;
LOCAL int		last_y;
LOCAL int		last_size;


/* Print a single character into overlay frame buffer */

/* x = 0..79  y= 0..23 */

/* xnor = 0 -> replace mode
   xnor = 1 -> xnor mode */

/* prints 12x10 characters */

LOCAL void pro_overlay_print_char(int x, int y, int xnor, int font, char ch)
{
int	sx, sy, sx0, sy0; /* screen coordinates */
int	i, pix, pnum, opix, vindex;
int	charint;


	charint = ((int)ch) - PRO_FONT_FIRSTCHAR;
	if ((charint < 0) || (charint>(PRO_FONT_NUMCHARS-1)))
	  charint = 32 - PRO_FONT_FIRSTCHAR;

	sx0 = x * 12;
	sy0 = y * 10;

	/* Render character */

	for(y=0; y<10; y++)
	{
	  sy = sy0 + y;

	  for(x=0; x<12; x++)
	  {
	    sx = sx0 + x;

	    /* Set color */

	    if (((pro_overlay_font[font][charint][y] >> (11-x)) & 01) == 0)
	      pix = blackpixel;
	    else
	      pix = whitepixel;

	    /* Plot pixel */

	    pnum = sy*PRO_VID_SCRWIDTH+sx;

	    /* Perform XNOR, if required */

	    if (xnor == 1)
	    {
	      /* Get old pixel value */

	      opix = 0;

	      for(i=0; i<pixsize; i++)
	        opix |= pro_overlay_data[pixsize*pnum+i] << (i*8);

	      if (opix == blackpixel)
	      {
	        if (pix == blackpixel)
	          pix = whitepixel;
	        else
	          pix = blackpixel;
	      }
	    }

	    /* Assign alpha based on color */

	    if (pix == blackpixel)
	      pro_overlay_alpha[sy*PRO_VID_SCRWIDTH+sx] = overlay_a;
	    else
	      /* Make white pixels non-transparent */

	      pro_overlay_alpha[sy*PRO_VID_SCRWIDTH+sx] = PRO_OVERLAY_MAXA;

	    /* Write pixel into frame buffer */

	    for(i=0; i<pixsize; i++)
	      pro_overlay_data[pixsize*pnum+i] = pix >> (i*8);

	    /* Mark display cache entry invalid */

	    vindex = vmem((sy<<6) | ((sx>>4)&077));
	    pro_vid_mvalid[cmem(vindex)] = 0;
	  }
	}
}


/* Print text string into overlay frame buffer */

void pro_overlay_print(int x, int y, int xnor, int font, char *text)
{
int	i, size;


	if (pro_overlay_open)
	{
	  if (x == -1)
	    x = start_x;
	  else if (x == -2)
	    x = last_x + last_size;
	  else
	    start_x = x;

	  if (y == -1)
	    y = last_y + 1;
	  else if (y == -2)
	    y = last_y;

	  if (y > 23)
	    y = 23;

	  size = strlen(text);

	  for(i=0; i<size; i++)
	    pro_overlay_print_char(x+i, y, xnor, font, text[i]);

	  last_x = x;
	  last_y = y;
	  last_size = size;
	}
}


/* Clear the overlay frame buffer */

void pro_overlay_clear ()
{
int	i, j;

	if (pro_overlay_open)
	{
	  for(i=0; i<PRO_VID_SCRWIDTH*PRO_VID_MEMHEIGHT; i++)
	  {
	    for(j=0; j<pixsize; j++)
	      pro_overlay_data[pixsize*i+j] = blackpixel >> (j*8);

	    pro_overlay_alpha[i] = 0;
	  }
	}
}


/* Turn on overlay */

void pro_overlay_enable ()
{
	pro_overlay_clear();
	pro_overlay_on = 1;
}


/* Turn off overlay */

void pro_overlay_disable ()
{
	pro_clear_mvalid();
	pro_overlay_on = 0;
}


/* Initialize the overlay frame buffer */

void pro_overlay_init (int psize, int cmode, int bpixel, int wpixel)
{
	if (pro_overlay_open == 0)
	{
	  start_x = 0;
	  last_x = 0;
	  last_y = 0;
	  last_size = 0;

	  clutmode = cmode;
	  pixsize = psize;
	  blackpixel = bpixel;
	  whitepixel = wpixel;

	  /* No blending is done in 8-bit modes */

	  if (cmode == 1)
	    overlay_a = PRO_OVERLAY_MAXA;
	  else
	    overlay_a = PRO_OVERLAY_A;

	  pro_overlay_data = (char *)malloc(PRO_VID_SCRWIDTH*PRO_VID_MEMHEIGHT*pixsize);
	  pro_overlay_alpha = (char *)malloc(PRO_VID_SCRWIDTH*PRO_VID_MEMHEIGHT);
	  pro_overlay_on = 0;
	  pro_overlay_open = 1;
	}
}


/* Close the overlay frame buffer */

void pro_overlay_close ()
{
	if (pro_overlay_open)
	{
	  free(pro_overlay_data);
	  free(pro_overlay_alpha);
	  pro_overlay_on = 0;
	  pro_overlay_open = 0;
	}
}
#endif
