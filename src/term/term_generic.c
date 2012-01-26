/* term_generic.c: SDL terminal code (screen update and keyboard)

   Copyright (c) 2011, Ismail Khatib (ikhatib@gmail.com)

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

#include "../pro_defs.h"

#include "term_gfx.h"


/* Default variables */
int	pro_window_x = 0;				/* window x position */
int	pro_window_y = 0;				/* window y position */

int	pro_screen_pcm = 1;				/* 0 = don't use PCM, 1 = use PCM (private color map) */
int	pro_screen_framebuffers = 1;	/* number of DGA framebuffers (1..3) */
int	pro_screen_full = 0;			/* 0 = window, 1 = full-screen */
int	pro_screen_window_scale = 2;	/* vertical window scale factor */
int	pro_screen_full_scale = 2;		/* vertical DGA scale factor */
int	pro_screen_gamma = 10;			/* 10x gamma correction factor */

int	pro_mouse_x = 0;				/* mouse x */
int	pro_mouse_y = 0;				/* mouse y */
int	pro_mouse_l = 0;				/* left mouse button */
int	pro_mouse_m = 0;				/* middle mouse button */
int	pro_mouse_r = 0;				/* right mouse button */
int	pro_mouse_in = 0;				/* mouse in window */


/* External keyboard polling routine */
int pro_keyboard_get () {
	return pro_gfx_current_driver->keyboard_get();
}

/* Turn on keyclick */
void pro_keyboard_click_on () {
	pro_gfx_current_driver->keyboard_click_on();
}

/* Turn off keyclick */
void pro_keyboard_click_off () {
	pro_gfx_current_driver->keyboard_click_off();
}

/* Turn on auto-repeat */
void pro_keyboard_auto_on () {
	pro_gfx_current_driver->keyboard_auto_on();
}

/* Turn off auto-repeat */
void pro_keyboard_auto_off () {
	pro_gfx_current_driver->keyboard_auto_off();
}

/* Sound keyboard bell */
void pro_keyboard_bell () {
	pro_gfx_current_driver->keyboard_bell();
}

/* Set keyboard bell volume */
void pro_keyboard_bell_vol (int vol) {
	pro_gfx_current_driver->keyboard_bell_vol(vol);
}


/* Turn on overlay */
void pro_overlay_enable () {
	pro_gfx_current_driver->overlay_enable();
}

/* Turn off overlay */
void pro_overlay_disable () {
	pro_gfx_current_driver->overlay_disable();
}

/* Print text string into overlay frame buffer */
void pro_overlay_print(int x, int y, int xnor, int font, char *text) {
	pro_gfx_current_driver->overlay_print(x, y, xnor, font, text);
}


/* Initialize the display */
int pro_screen_init () {
	if (pro_gfx_current_driver && pro_gfx_current_driver->screen_init) {
		return pro_gfx_current_driver->screen_init();
	} else {
		return PRO_FAIL;
	}
}

/* Closes the display */
void pro_screen_close () {
	if (pro_gfx_current_driver && pro_gfx_current_driver->screen_close) {
		pro_gfx_current_driver->screen_close();
	}
}

/* Put a title on the display window */
void pro_screen_title (char *title) {
	pro_gfx_current_driver->screen_title(title);
}

/* This is called every emulated vertical retrace */
void pro_screen_update () {
	pro_gfx_current_driver->screen_update();
}

/* Reset routine (called only once) */
void pro_screen_reset () {
	if (pro_gfx_current_driver && pro_gfx_current_driver->screen_reset) {
		pro_gfx_current_driver->screen_reset();
	}
}

/* This is called whenever the scroll register changes */
void pro_scroll () {
	pro_gfx_current_driver->scroll();
}


/* This function is called whenever the colormap mode changes. */
void pro_mapchange () {
	pro_gfx_current_driver->mapchange();
}

/* This writes an 8-bit (3-3-2) RGB value into the PRO's colormap */
void pro_colormap_write (int index, int rgb) {
	pro_gfx_current_driver->colormap_write(index, rgb);
}


void pro_gfx_drivers_init () {
	if (pro_gfx_current_driver) {
		pro_gfx_current_driver->gfx_driver_init();
	}
}

#endif
