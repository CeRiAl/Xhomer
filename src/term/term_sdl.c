/* term_sdl.c: SDL terminal code (screen update and keyboard)

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

#ifdef HAS_SDL

#include <SDL.h>
#include <SDL_syswm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "../pro_defs.h"
#include "../pro_lk201.h"

#include "../pro_font.h"

#include "term_gfx.h"

#define USE_OPENGL

#define	PRO_KEYBOARD_FIFO_DEPTH	1024

/* Dummy values copied from term_x11.c - required so xhomer can compile. */
LOCAL int	pro_screen_open = 0;
LOCAL int	pro_screen_winheight;		/* height of framebuffer */
LOCAL int	pro_screen_bufheight;		/* height of allocated framebuffer */
LOCAL int	pro_screen_updateheight;	/* height to update */

LOCAL int	pro_keyboard_fifo_h;		/* FIFO head pointer */
LOCAL int	pro_keyboard_fifo_t;		/* FIFO tail pointer */
LOCAL int	pro_keyboard_fifo[PRO_KEYBOARD_FIFO_DEPTH];

LOCAL SDL_Color	ProSDLColor_map[8];
LOCAL SDL_Color	ProSDLColor_nonmap[8];
LOCAL int	ProSDLDepth;	/* color depth */

LOCAL SDL_Surface	*ProSDLScreen;
LOCAL SDL_SysWMinfo	ProSDLWindow;	/* window structure */

LOCAL SDL_Surface	*pro_sdl_image;
LOCAL SDL_Surface	*pro_sdl_overlay_data;	/* overlay frame buffer */

LOCAL int	pro_sdl_keyboard_keys[SDLK_LAST];	/* keystate */

LOCAL int	pro_screen_blank = 0;	/* indicates whether screen is blanked */

LOCAL SDL_Color	*pro_sdl_lut;			/* points to nonmap or colormap */

LOCAL char* current_title = NULL;
LOCAL int title_needs_update = 0;

LOCAL int	pro_sdl_overlay_on = 0;

LOCAL int	pro_overlay_open = 0;
LOCAL int	start_x;
LOCAL int	last_x;
LOCAL int	last_y;
LOCAL int	last_size;

LOCAL int	sdl_blackpixel;
LOCAL int	sdl_whitepixel;

/* Print a single character into overlay frame buffer */
/* x = 0..79  y= 0..23 */
/* xnor = 0 -> replace mode
   xnor = 1 -> xnor mode */

/* prints 12x10 characters */
LOCAL void pro_sdl_overlay_print_char(int x, int y, int xnor, int font, char ch)
{
int	sx, sy, sx0, sy0; /* screen coordinates */
int	vindex;
int	charint;

int sdl_pix, sdl_opix;
int sdl_bpp = pro_sdl_overlay_data->format->BytesPerPixel;


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
	    if (((pro_overlay_font[font][charint][y] >> (11-x)) & 01) == 0) {
	      sdl_pix = sdl_blackpixel;
	    } else {
	      sdl_pix = sdl_whitepixel;
	    }

	    /* Plot pixel */
	    /* Perform XNOR, if required */
	    if (xnor == 1)
	    {
	      Uint8 *p = (Uint8 *)pro_sdl_overlay_data->pixels + sy * pro_sdl_overlay_data->pitch + sx * sdl_bpp;
	      switch(sdl_bpp) {
	        case 1:
	          sdl_opix = *p;
	          break;
	        case 2:
	          sdl_opix = *(Uint16 *)p;
	          break;
	        case 3:
	          if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
	        	sdl_opix = p[0] << 16 | p[1] << 8 | p[2];
	          else
	        	sdl_opix = p[0] | p[1] << 8 | p[2] << 16;
	          break;
	        case 4:
	          sdl_opix = *(Uint32 *)p;
	          break;
	        default:
	          sdl_opix = 0;       /* shouldn't happen, but avoids warnings */
	          break;
	      }

	      if (sdl_opix == sdl_blackpixel) {
	    	if (sdl_pix == sdl_blackpixel)
	    	  sdl_pix = sdl_whitepixel;
	    	else
		      sdl_pix = sdl_blackpixel;
	      }
	    }

	    /* Write pixel into frame buffer */

	    Uint8 *p = (Uint8 *)pro_sdl_overlay_data->pixels + sy * pro_sdl_overlay_data->pitch + sx * sdl_bpp;
	    switch(sdl_bpp) {
	      case 1:
	        *p = sdl_pix;
	        break;
	      case 2:
	        *(Uint16 *)p = sdl_pix;
	        break;
	      case 3:
	        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	          p[0] = (sdl_pix >> 16) & 0xff;
	          p[1] = (sdl_pix >> 8) & 0xff;
	          p[2] = sdl_pix & 0xff;
	        } else {
	          p[0] = sdl_pix & 0xff;
	          p[1] = (sdl_pix >> 8) & 0xff;
	          p[2] = (sdl_pix >> 16) & 0xff;
	        }
	        break;
	      case 4:
	          *(Uint32 *)p = sdl_pix;
	          break;
	    }

	    /* Mark display cache entry invalid */
	    vindex = vmem((sy<<6) | ((sx>>4)&077));
	    pro_vid_mvalid[cmem(vindex)] = 0;
	  }
	}
}


/* Print text string into overlay frame buffer */
void pro_sdl_overlay_print(int x, int y, int xnor, int font, char *text)
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
	    pro_sdl_overlay_print_char(x+i, y, xnor, font, text[i]);

	  last_x = x;
	  last_y = y;
	  last_size = size;
	}
}


/* Clear the overlay frame buffer */
void pro_sdl_overlay_clear ()
{
	if (pro_overlay_open)
	{
	  SDL_FillRect(pro_sdl_overlay_data, NULL, SDL_MapRGBA(pro_sdl_overlay_data->format, 0, 0, 0, 0));
	}
}


/* Turn on overlay */
void pro_sdl_overlay_enable ()
{
	pro_sdl_overlay_clear();
	pro_sdl_overlay_on = 1;
}


/* Turn off overlay */
void pro_sdl_overlay_disable ()
{
	pro_clear_mvalid();
	pro_sdl_overlay_on = 0;
}


/* Initialize the overlay frame buffer */
void pro_sdl_overlay_init (int psize, int cmode, int bpixel, int wpixel)
{
	if (pro_overlay_open == 0)
	{
	  start_x = 0;
	  last_x = 0;
	  last_y = 0;
	  last_size = 0;

	  Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif
	  pro_sdl_overlay_data = SDL_CreateRGBSurface( (SDL_SWSURFACE | SDL_SRCALPHA), PRO_VID_SCRWIDTH, PRO_VID_SCRHEIGHT, ProSDLDepth, rmask, gmask, bmask, amask);

	  sdl_blackpixel = SDL_MapRGBA(pro_sdl_overlay_data->format, 0, 0, 0, (255-PRO_OVERLAY_A));
	  sdl_whitepixel = SDL_MapRGBA(pro_sdl_overlay_data->format, 255, 255, 255, 255);

	  pro_sdl_overlay_on = 0;
	  pro_overlay_open = 1;
	}
}


/* Close the overlay frame buffer */
void pro_sdl_overlay_close ()
{
	if (pro_overlay_open)
	{
	  SDL_FreeSurface(pro_sdl_overlay_data);
	  pro_sdl_overlay_on = 0;
	  pro_overlay_open = 0;
	}
}


/* Put a title on the display window */
void pro_sdl_screen_title (char *title)
{
	if (current_title)
		free(current_title);

	current_title = strdup(title);
	title_needs_update = 1;
}


/* Keycode lookup routine (key press) */
LOCAL int pro_keyboard_lookup_down (SDLKey sym)
{
	switch (sym)
	{
	  case SDLK_ESCAPE:			return PRO_LK201_HOLD;
	  case SDLK_F1:				return PRO_LK201_PRINT;
	  case SDLK_F2:				return PRO_LK201_SETUP;
	  case SDLK_F3:				return PRO_LK201_FFOUR;
	  case SDLK_F4:				return PRO_LK201_BREAK;
	  case SDLK_F5:				return PRO_LK201_INT;
	  case SDLK_F6:				return PRO_LK201_RESUME;
	  case SDLK_F7:				return PRO_LK201_CANCEL;
	  case SDLK_F8:				return PRO_LK201_MAIN;
	  case SDLK_F9:				return PRO_LK201_EXIT;
	  case SDLK_F10:			return PRO_LK201_ESC;
	  case SDLK_F11:			return PRO_LK201_BS;
	  case SDLK_F12:			return PRO_LK201_LF;
	  case SDLK_NUMLOCK:
	  case SDLK_SYSREQ:			return PRO_LK201_ADDOP;
	  case SDLK_LMETA:			return PRO_LK201_HELP;
	  case SDLK_PAUSE:			return PRO_LK201_DO;

//	  case SDLK_a:				return PRO_LK201_A;
	  case SDLK_a:				return PRO_LK201_ADDOP;
	  case SDLK_b:				return PRO_LK201_B;
	  case SDLK_c:				return PRO_LK201_C;
	  case SDLK_d:				return PRO_LK201_D;
	  case SDLK_e:				return PRO_LK201_E;
	  case SDLK_f:				return PRO_LK201_F;
	  case SDLK_g:				return PRO_LK201_G;
	  case SDLK_h:				return PRO_LK201_H;
	  case SDLK_i:				return PRO_LK201_I;
	  case SDLK_j:				return PRO_LK201_J;
	  case SDLK_k:				return PRO_LK201_K;
	  case SDLK_l:				return PRO_LK201_L;
	  case SDLK_m:				return PRO_LK201_M;
	  case SDLK_n:				return PRO_LK201_N;
	  case SDLK_o:				return PRO_LK201_O;
	  case SDLK_p:				return PRO_LK201_P;
	  case SDLK_q:				return PRO_LK201_Q;
	  case SDLK_r:				return PRO_LK201_R;
	  case SDLK_s:				return PRO_LK201_S;
	  case SDLK_t:				return PRO_LK201_T;
	  case SDLK_u:				return PRO_LK201_U;
	  case SDLK_v:				return PRO_LK201_V;
	  case SDLK_w:				return PRO_LK201_W;
	  case SDLK_x:				return PRO_LK201_X;
	  case SDLK_y:				return PRO_LK201_Y;
	  case SDLK_z:				return PRO_LK201_Z;

	  case SDLK_0:				return PRO_LK201_0;
	  case SDLK_1:				return PRO_LK201_1;
	  case SDLK_2:				return PRO_LK201_2;
	  case SDLK_3:				return PRO_LK201_3;
	  case SDLK_4:				return PRO_LK201_4;
	  case SDLK_5:				return PRO_LK201_5;
	  case SDLK_6:				return PRO_LK201_6;
	  case SDLK_7:				return PRO_LK201_7;
	  case SDLK_8:				return PRO_LK201_8;
	  case SDLK_9:				return PRO_LK201_9;

	  case SDLK_MINUS:			return PRO_LK201_MINUS;
	  case SDLK_EQUALS:			return PRO_LK201_EQUAL;
	  case SDLK_LEFTBRACKET:	return PRO_LK201_LEFTB;
	  case SDLK_RIGHTBRACKET:	return PRO_LK201_RIGHTB;

	  case SDLK_SEMICOLON:		return PRO_LK201_SEMI;
	  case SDLK_QUOTE:			return PRO_LK201_QUOTE;
	  case SDLK_BACKSLASH:		return PRO_LK201_BACKSL;
	  case SDLK_COMMA:			return PRO_LK201_COMMA;
	  case SDLK_PERIOD:			return PRO_LK201_PERIOD;
	  case SDLK_SLASH:			return PRO_LK201_SLASH;
	  case SDLK_BACKQUOTE:		return PRO_LK201_TICK;

	  case SDLK_SPACE:			return PRO_LK201_SPACE;

	  case SDLK_BACKSPACE:		return PRO_LK201_DEL;
	  case SDLK_RETURN:			return PRO_LK201_RETURN;
	  case SDLK_TAB:			return PRO_LK201_TAB;

	  case SDLK_UP:				return PRO_LK201_UP;
	  case SDLK_DOWN:			return PRO_LK201_DOWN;
	  case SDLK_LEFT:			return PRO_LK201_LEFT;
	  case SDLK_RIGHT:			return PRO_LK201_RIGHT;

	  case SDLK_INSERT:			return PRO_LK201_FIND;
	  case SDLK_HOME:			return PRO_LK201_INSERT;
	  case SDLK_PAGEUP:			return PRO_LK201_REMOVE;
	  case SDLK_DELETE:			return PRO_LK201_SELECT;
	  case SDLK_END:			return PRO_LK201_PREV;
	  case SDLK_PAGEDOWN:		return PRO_LK201_NEXT;

	  case SDLK_CAPSLOCK:		return PRO_LK201_LOCK;
	  case SDLK_LSHIFT:			return PRO_LK201_SHIFT;
	  case SDLK_RSHIFT:			return PRO_LK201_SHIFT;
	  case SDLK_LCTRL:			return PRO_LK201_CTRL;
	  case SDLK_RCTRL:			return PRO_LK201_CTRL;
	  case SDLK_LALT:			return PRO_LK201_COMPOSE;

	  default:					return PRO_NOCHAR;
	}
}


/* Keycode lookup routine (key release) */
LOCAL int pro_keyboard_lookup_up (SDLKey sym)
{
	switch (sym)
	{
	  case SDLK_LSHIFT:	return PRO_LK201_ALLUPS;
	  case SDLK_RSHIFT:	return PRO_LK201_ALLUPS;
	  case SDLK_LCTRL:	return PRO_LK201_ALLUPS;
	  case SDLK_RCTRL:	return PRO_LK201_ALLUPS;

	  default:			return PRO_NOCHAR;
	}
}


/* Put character in keycode FIFO */
LOCAL void pro_keyboard_fifo_put (int key)
{
	/* Filter keycodes for commands */
	/* This is to avoid changing the menu state while screen is closed */
	if (pro_screen_open)
	  key = pro_menu(key);

	if (key != PRO_NOCHAR)
	{
	  /* First check if FIFO is full */
	  if ((pro_keyboard_fifo_h != (pro_keyboard_fifo_t-1))
	      && ((pro_keyboard_fifo_h != (PRO_KEYBOARD_FIFO_DEPTH-1))
	          || (pro_keyboard_fifo_t != 0)))
	  {
	    pro_keyboard_fifo[pro_keyboard_fifo_h] = key;
	    pro_keyboard_fifo_h++;
	    if (pro_keyboard_fifo_h == PRO_KEYBOARD_FIFO_DEPTH)
	      pro_keyboard_fifo_h = 0;
	  }
	}
}


/* External keyboard polling routine */
int pro_sdl_keyboard_get ()
{
int	key;
	

	/* Get character from keycode FIFO */
	/* Check if FIFO is empty */
	if (pro_keyboard_fifo_h == pro_keyboard_fifo_t)	
	  key = PRO_NOCHAR;
	else
	{
	  key = pro_keyboard_fifo[pro_keyboard_fifo_t];
	  pro_keyboard_fifo_t++;
	  if (pro_keyboard_fifo_t == PRO_KEYBOARD_FIFO_DEPTH)
	    pro_keyboard_fifo_t = 0;
	}

	return key;
}


/* Save keymap */
void pro_sdl_screen_save_keys ()
{
int	i;
unsigned char *keys = SDL_GetKeyState(NULL);


	for(i=0; i<SDLK_LAST; i++)
		pro_sdl_keyboard_keys[i] = keys[i];
}


/* Bring emulator state up to date with shift/ctrl key state that may have changed
   while focus was lost */
void pro_sdl_screen_update_keys ()
{
int	i, key, oldkey, curkey;
unsigned char *keys = SDL_GetKeyState(NULL);


	for(i=0; i<SDLK_LAST; i++)
	{
	  oldkey = pro_sdl_keyboard_keys[i];
	  curkey = keys[i];

	  if (oldkey != curkey)
	  {
	    if (curkey == 0)
	      key = pro_keyboard_lookup_up(key);
	    else
	      key = pro_keyboard_lookup_down(key);

	    if ((key == PRO_LK201_SHIFT) || (key == PRO_LK201_CTRL)
	        || (key == PRO_LK201_ALLUPS))
	      pro_keyboard_fifo_put(key);
	  }
	}
}


/* Initialize the display */
int pro_sdl_screen_init ()
{
char	window_pos_env[256];


	if (pro_screen_open == 0)
	{
	  // Set window coordinates
	  sprintf(window_pos_env, "SDL_VIDEO_WINDOW_POS=%d,%d", pro_window_x, pro_window_y);
	  SDL_putenv(window_pos_env);

	  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		  fprintf(stderr, "Could not initialize SDL: %s\r\n", SDL_GetError());
		  return PRO_FAIL;
	  }

	  pro_screen_bufheight = PRO_VID_SCRHEIGHT;
	  pro_screen_winheight = pro_screen_window_scale*pro_screen_bufheight;
	  pro_screen_updateheight = PRO_VID_SCRHEIGHT;

#ifdef USE_OPENGL
	  SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	  ProSDLScreen = SDL_SetVideoMode(PRO_VID_SCRWIDTH, pro_screen_winheight, 0, SDL_SWSURFACE | SDL_OPENGLBLIT);
#else
	  ProSDLScreen = SDL_SetVideoMode(PRO_VID_SCRWIDTH, pro_screen_winheight, 0, SDL_SWSURFACE);
#endif

	  ProSDLDepth = ProSDLScreen->format->BitsPerPixel;

	  SDL_VERSION(&ProSDLWindow.version);
	  SDL_GetWMInfo(&ProSDLWindow);

	  // Determine whether window has input focus
	  if (SDL_GetAppState() & SDL_APPMOUSEFOCUS)
	    pro_mouse_in = 1;
	  else
	    pro_mouse_in = 0;

	  pro_screen_title("Pro 350");

	  // Create image buffer for screen updates
	  pro_sdl_image = SDL_CreateRGBSurface( SDL_SWSURFACE, PRO_VID_SCRWIDTH, PRO_VID_SCRHEIGHT, ProSDLDepth, 0, 0, 0, 0);

/*
#ifdef USE_OPENGL
	  SDL_UpdateRect(ProSDLScreen, 0, 0, 0, 0);
	  SDL_GL_SwapBuffers();
#else
	  SDL_Flip(ProSDLScreen);
#endif
*/
	  // Make emulator state consistent with keyboard state
	  pro_sdl_screen_update_keys();

	  // Initialize the overlay frame buffer
	  pro_sdl_overlay_init(0, 0, 0, 0);

	  pro_screen_open = 1;

	  // Clear image buffer
	  SDL_FillRect(pro_sdl_image, NULL, SDL_MapRGB(pro_sdl_image->format, 0, 0, 0));

	  // Invalidate display cache
	  pro_clear_mvalid();
	}

	return PRO_SUCCESS;
}


void pro_sdl_screen_close ()
{
	if (pro_screen_open)
	{
		pro_screen_open = 0;

		/* Save keymap */
		pro_sdl_screen_save_keys();

		SDL_FreeSurface (pro_sdl_image);

		SDL_FreeSurface (ProSDLScreen);

		/* Close the overlay frame buffer */
		pro_sdl_overlay_close();
	}
}


/* Reset routine (called only once) */
void pro_sdl_screen_reset ()
{
int	r, g, b, i;


	/* Clear keymap */
	memset(&pro_sdl_keyboard_keys, 0, sizeof(pro_sdl_keyboard_keys));

	/* Initialize colormaps */
	for(r=0; r<=1; r++)
	  for(g=0; g<=1; g++)
	    for(b=0; b<=1; b++)
	    {
	      i = (r | (g<<1) | (b<<2));

	      ProSDLColor_nonmap[i].r = r * 255;
	      ProSDLColor_nonmap[i].g = g * 255;
	      ProSDLColor_nonmap[i].b = b * 255;

	      ProSDLColor_map[i].r = 0;
	      ProSDLColor_map[i].g = 0;
	      ProSDLColor_map[i].b = 0;
	    }
}


/* This function is called whenever the colormap mode changes. */
void pro_sdl_mapchange ()
{
	pro_clear_mvalid();
}


/* This writes an 8-bit (3-3-2) RGB value into the PRO's colormap */
void pro_sdl_colormap_write (int index, int rgb)
{
double	r, g, b;

#if PRO_EBO_PRESENT
	/* Calculate linear color component values 0.0 - 1.0 */
	r = (double)((rgb & PRO_VID_R_MASK) >> 5)/7.0;
	g = (double)((rgb & PRO_VID_G_MASK) >> 2)/7.0;
	b = (double)(rgb & PRO_VID_B_MASK)/3.0;

	/* Perform gamma correction */
	r = pow(r, 10.0/(double)pro_screen_gamma);
	g = pow(g, 10.0/(double)pro_screen_gamma);
	b = pow(b, 10.0/(double)pro_screen_gamma);

	ProSDLColor_map[index].r = (int)(r * 255);
	ProSDLColor_map[index].g = (int)(g * 255);
	ProSDLColor_map[index].b = (int)(b * 255);

	/* Check if in colormapped mode */
	if ((pro_vid_csr & PRO_VID_CME) != 0)
	{
	  /* The cache gets cleared only for non-private colormap X11 modes */
	  pro_clear_mvalid();
	}
#endif
}


/* This is called whenever the scroll register changes */
void pro_sdl_scroll ()
{
	  pro_clear_mvalid();
}


/* Service X events */
void pro_sdl_screen_service_events ()
{
SDL_Event sdl_event;
int		sdl_expose = 0;
int		key;


	/* If multiple events are buffered up, flush them all out */
	while(SDL_PollEvent(&sdl_event)) {
		switch(sdl_event.type) {
			case SDL_VIDEOEXPOSE:
				sdl_expose = 1;
				break;
			case SDL_KEYDOWN:
				key = sdl_event.key.keysym.sym;
				key = pro_keyboard_lookup_down(key);
				if (key != PRO_NOCHAR)
					pro_keyboard_fifo_put(key);
				break;
			case SDL_KEYUP:
				key = sdl_event.key.keysym.sym;
				key = pro_keyboard_lookup_up(key);
				if (key != PRO_NOCHAR)
					pro_keyboard_fifo_put(key);
				break;
			case SDL_MOUSEMOTION:
		        pro_mouse_x = sdl_event.motion.x;
		        pro_mouse_y = sdl_event.motion.y/pro_screen_window_scale;
				break;
			case SDL_MOUSEBUTTONDOWN:
				switch(sdl_event.button.button) {
					case SDL_BUTTON_LEFT:
					  pro_mouse_l = 1;
					  break;
					case SDL_BUTTON_MIDDLE:
					  pro_mouse_m = 1;
					  break;
					case SDL_BUTTON_RIGHT:
					  pro_mouse_r = 1;
					  break;
				}
				break;
			case SDL_MOUSEBUTTONUP:
				switch(sdl_event.button.button) {
					case SDL_BUTTON_LEFT:
					  pro_mouse_l = 0;
					  break;
					case SDL_BUTTON_MIDDLE:
					  pro_mouse_m = 0;
					  break;
					case SDL_BUTTON_RIGHT:
					  pro_mouse_r = 0;
					  break;
				}
				break;
			case SDL_ACTIVEEVENT:
				if (sdl_event.active.gain) {
			        pro_mouse_in = 1;
			        pro_sdl_screen_update_keys();
				} else {
			        pro_sdl_screen_save_keys();
			        pro_mouse_in = 0;
				}
				break;
			case SDL_QUIT:
				pro_exit();
				break;
		}
	}

	/* Check if screen should be updated */
	if (sdl_expose)
	{
	  /* Clear the window */
      SDL_FillRect(ProSDLScreen, NULL, SDL_MapRGB(ProSDLScreen->format, 0, 0, 0));

	  /* Clear display cache, forcing full update */
	  pro_clear_mvalid();
	}
}


/* This is called every emulated vertical retrace */
void pro_sdl_screen_update ()
{
int	i, vdata0, vdata1, vdata2, vindex, vpix, vpixs, vpixe, x, y;
int	cindex;
int	offset, reps;

SDL_Rect sdl_srcrect;
SDL_Rect sdl_dstrect;
int sdl_color;
int sdl_bpp = pro_sdl_image->format->BytesPerPixel;
int flip_needed = 0;

	/* Service SDL events */

	pro_sdl_screen_service_events();

	offset = 0;
	reps = 1;
	vindex = vmem(0);

	/* Check whether screen has been blanked */

	if ((pro_vid_p1c & PRO_VID_P1_HRS) == PRO_VID_P1_OFF)
	{
	  if (pro_screen_blank == 0)
	  {
	    /* Blank the screen */
	    SDL_FillRect(ProSDLScreen, NULL, SDL_MapRGB(ProSDLScreen->format, 0, 0, 0));

	    pro_screen_blank = 1;
	  }
	}
	else
	{
	  if (pro_screen_blank == 1)
	  {
	    /* Unblank the screen */

	    pro_clear_mvalid();
	    pro_screen_blank = 0;
	  }

#if PRO_EBO_PRESENT
	  /* Determine whether system is in color mapped mode */

	  if ((pro_vid_csr & PRO_VID_CME) != 0)
	    pro_sdl_lut = ProSDLColor_map;
	  else
	    pro_sdl_lut = ProSDLColor_nonmap;

#else
	  pro_sdl_lut = ProSDLColor_nonmap;
#endif

	  /* Redraw portions of screen that have changed */

	  for(y=0; y<pro_screen_updateheight; y++)
	  {
	    for(x=0; x<PRO_VID_SCRWIDTH; x+=PRO_VID_CLS_PIX)
	    {
	      /* Update screen segment only if display cache is invalid */

	      if (pro_vid_mvalid[cmem(vindex)] == 0)
	      {
	      flip_needed = 1;
	      if (pro_sdl_overlay_on == 0)
	        for(i=0; i<(PRO_VID_CLS_PIX/16); i++)
	          {
	            vpixs = i * 16;
	            vpixe = vpixs + 16;

	            vdata0 = PRO_VRAM[0][vindex+i];
	            vdata1 = PRO_VRAM[1][vindex+i] << 1;
	            vdata2 = PRO_VRAM[2][vindex+i] << 2;

	            for(vpix=vpixs; vpix<vpixe; vpix++)
	            {
	              cindex = (vdata2 & 04) | (vdata1 & 02) | (vdata0 & 01);
	              sdl_color = SDL_MapRGB(pro_sdl_image->format, pro_sdl_lut[cindex].r, pro_sdl_lut[cindex].g, pro_sdl_lut[cindex].b);

	              Uint8 *p = (Uint8 *)pro_sdl_image->pixels + y * pro_sdl_image->pitch + vpix * sdl_bpp;
	              switch(sdl_bpp) {
	                case 1:
	              	  *p = sdl_color;
	                  break;
	                case 2:
	                  *(Uint16 *)p = sdl_color;
	                  break;
	                case 3:
	                  if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	                    p[0] = (sdl_color >> 16) & 0xff;
	                    p[1] = (sdl_color >> 8) & 0xff;
	                    p[2] = sdl_color & 0xff;
	                  } else {
	                    p[0] = sdl_color & 0xff;
	                    p[1] = (sdl_color >> 8) & 0xff;
	                    p[2] = (sdl_color >> 16) & 0xff;
	                  }
	                  break;
	                case 4:
	                  *(Uint32 *)p = sdl_color;
	                  break;
	              }

	              vdata0 = vdata0 >> 1;
	              vdata1 = vdata1 >> 1;
	              vdata2 = vdata2 >> 1;
	            }
	          }
	        else {

	          /* XXX putting this in a function might speed things up */

	          for(i=0; i<(PRO_VID_CLS_PIX/16); i++)
	          {
	            vpixs = i * 16;
	            vpixe = vpixs + 16;

	            vdata0 = PRO_VRAM[0][vindex+i];
	            vdata1 = PRO_VRAM[1][vindex+i] << 1;
	            vdata2 = PRO_VRAM[2][vindex+i] << 2;

	            for(vpix=vpixs; vpix<vpixe; vpix++)
	            {
	              cindex = (vdata2 & 04) | (vdata1 & 02) | (vdata0 & 01);
	              sdl_color = SDL_MapRGB(pro_sdl_image->format, pro_sdl_lut[cindex].r, pro_sdl_lut[cindex].g, pro_sdl_lut[cindex].b);
  
	              Uint8 *p = (Uint8 *)pro_sdl_image->pixels + y * pro_sdl_image->pitch + vpix * sdl_bpp;
	              switch(sdl_bpp) {
	                case 1:
	              	  *p = sdl_color;
	                  break;
	                case 2:
	                  *(Uint16 *)p = sdl_color;
	                  break;
	                case 3:
	                  if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	                    p[0] = (sdl_color >> 16) & 0xff;
	                    p[1] = (sdl_color >> 8) & 0xff;
	                    p[2] = sdl_color & 0xff;
	                  } else {
	                    p[0] = sdl_color & 0xff;
	                    p[1] = (sdl_color >> 8) & 0xff;
	                    p[2] = (sdl_color >> 16) & 0xff;
	                  }
	                  break;
	                case 4:
	                  *(Uint32 *)p = sdl_color;
	                  break;
	              }

	              vdata0 = vdata0 >> 1;
	              vdata1 = vdata1 >> 1;
	              vdata2 = vdata2 >> 1;
	            }
	          }
	        }

	        /* Draw cache-line on screen */

		    sdl_srcrect.x = x;
		    sdl_srcrect.y = y;
		    sdl_srcrect.w = PRO_VID_CLS_PIX;
		    sdl_srcrect.h = 1;

		    sdl_dstrect.x = x;
		    sdl_dstrect.y = y*pro_screen_window_scale;
		    sdl_dstrect.w = PRO_VID_CLS_PIX;
		    sdl_dstrect.h = 1;

		    if (pro_sdl_overlay_on == 1)
		      SDL_BlitSurface(pro_sdl_overlay_data, NULL, pro_sdl_image, NULL);

		    SDL_BlitSurface(pro_sdl_image, &sdl_srcrect, ProSDLScreen, &sdl_dstrect);

	        /* Mark cache entry valid */

	        pro_vid_mvalid[cmem(vindex)] = 1;
	      }

	      vindex = (vindex + (PRO_VID_CLS_PIX/16)) & PRO_VID_VADDR_MASK;
	    }
	  }

	  if (flip_needed == 1) {
#ifdef USE_OPENGL
		  SDL_UpdateRect(ProSDLScreen, 0, 0, 0, 0);
		  SDL_GL_SwapBuffers();
#else
		  SDL_Flip(ProSDLScreen);
#endif
	  }
	}

	/* Check if title needs to be updated */

	if (title_needs_update)
	{
	  title_needs_update = 0;
	  SDL_WM_SetCaption(current_title, current_title);
	}
}


/* Set keyboard bell volume */
void pro_sdl_keyboard_bell_vol (int vol)
{
	/* vol is in the range 0 (loudest) to 7 (softest) */
//	pro_keyboard_control.bell_percent = (100*(7-vol))/7;
}


/* Sound keyboard bell */
void pro_sdl_keyboard_bell ()
{
//	XBell(ProDisplay, 0);
}


/* Turn off auto-repeat */
void pro_sdl_keyboard_auto_off ()
{
//	pro_keyboard_control.auto_repeat_mode = AutoRepeatModeOff;
}


/* Turn on auto-repeat */
void pro_sdl_keyboard_auto_on ()
{
//	pro_keyboard_control.auto_repeat_mode = AutoRepeatModeOn;
}


/* Turn off keyclick */
void pro_sdl_keyboard_click_off ()
{
//	pro_keyboard_control.key_click_percent = 0;
}


/* Turn on keyclick */
void pro_sdl_keyboard_click_on ()
{
//	pro_keyboard_control.key_click_percent = 100;
}


/* Initialize this driver */
void pro_sdl_gfx_driver_init ()
{
	printf("Xhomer SDL Driver\r\n");
}


/* Graphic driver info and description */
pro_gfx_driver_t	pro_sdl_driver = {
	{"Xhomer SDL Driver"},
	pro_sdl_gfx_driver_init,

	pro_sdl_keyboard_get,
	pro_sdl_keyboard_click_on,
	pro_sdl_keyboard_click_off,
	pro_sdl_keyboard_auto_on,
	pro_sdl_keyboard_auto_off,
	pro_sdl_keyboard_bell,
	pro_sdl_keyboard_bell_vol,

	pro_sdl_overlay_enable,
	pro_sdl_overlay_disable,
	pro_sdl_overlay_print,

	pro_sdl_screen_init,
	pro_sdl_screen_close,
	pro_sdl_screen_title,
	pro_sdl_screen_update,
	pro_sdl_screen_reset,
	pro_sdl_scroll,

	pro_sdl_mapchange,
	pro_sdl_colormap_write
};

#endif /* HAS_SDL */

#endif
