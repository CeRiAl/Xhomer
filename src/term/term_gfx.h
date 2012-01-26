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

#ifndef TERM_GFX_H

#define TERM_GFX_H


/* Graphics "driver" structure */
struct pro_gfx_driver {
	char *gfx_driver_name[128];
	void (*gfx_driver_init) ();

	int (*keyboard_get) ();
	void (*keyboard_click_on) ();
	void (*keyboard_click_off) ();
	void (*keyboard_auto_on) ();
	void (*keyboard_auto_off) ();
	void (*keyboard_bell) ();
	void (*keyboard_bell_vol) (int vol);

	void (*overlay_enable) ();
	void (*overlay_disable) ();
	void (*overlay_print) (int x, int y, int xnor, int font, char *text);

	int (*screen_init) ();
	void (*screen_close) ();
	void (*screen_title) (char *title);
	void (*screen_update) ();
	void (*screen_reset) ();
	void (*scroll) ();

	void (*mapchange) ();
	void (*colormap_write) (int index, int rgb);
};
typedef struct pro_gfx_driver pro_gfx_driver_t;

pro_gfx_driver_t *pro_gfx_current_driver;

#ifdef HAS_CURSES
GLOBAL pro_gfx_driver_t pro_curses_driver;
#endif
#ifdef HAS_SDL
GLOBAL pro_gfx_driver_t pro_sdl_driver;
#endif
#ifdef HAS_X11
GLOBAL pro_gfx_driver_t pro_x11_driver;
#endif

#endif /* !TERM_GFX_H */

#endif
