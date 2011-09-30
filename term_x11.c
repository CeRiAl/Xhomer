/* term_x11.c: X-windows terminal code (screen update and keyboard)

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
		-alloc only 2 colors for non-ebo 8-bit
		-use better technique to detect CLUT modes
		-add DGA mode that only writes each pixel once
		-colormap is not loaded for DGA+PCM
		-get screensaver blanking to work during DGA switch

   Xserver DGA2 bugs:

   Currently, standard X event handling is used, as the DGA stuff is broken
   in DGA2

     XFree 4.0.2 Neomagic
       -Use of more than 4MB of graphics memory causes crash during modeswitch
       -Menu does not work if more than X MB of graphics memory is used
       -XDGA events are not being seen (see #ifdef DGAXXX)
       -XDGAFlipImmediate is not supported.  So, xhomer will wait until the
        viewport update has been completed, causing slower scrolling with
        this server.  Servers that support XDGAFlipImmediate are not
        affected.

     XFree 4.0.2 I128
       -XDGASetMode(..., ..., 0) does *not* return to initial screen mode
       -grabbing keyboard and mouse does not work, unless sleep(1) first
       -framebuffer height is incorrectly reported as that of single a screen
       -XDGA events are not being seen (see #ifdef DGAXXX)
       -Expose events are not sent in DGA mode
       -EnterNotify is sent, instead, *most* of the time
       -look for "pro_nine_workaround"
*/

#ifdef PRO

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef SHM
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#ifdef DGA
#include <X11/extensions/xf86dga.h>
#ifndef DGA2
#include <X11/extensions/xf86vmode.h>
#endif
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "pro_defs.h"
#include "pro_lk201.h"


#define	PRO_KEYBOARD_FIFO_DEPTH	1024

int			pro_nine_workaround = 0;	/* workaround for #9 Xserver bugs */
int			pro_libc_workaround = 0;	/* workaround for pre-glibc-2.0 bug */
int			pro_window_x = 0;		/* window x position */
int			pro_window_y = 0;		/* window y position */
int			pro_screen_full = 0;		/* 0 = window, 1 = full-screen */
int			pro_screen_window_scale = 2;	/* vertical window scale factor */
int			pro_screen_full_scale = 3;	/* vertical DGA scale factor */
int			pro_screen_gamma = 10;		/* 10x gamma correction factor */
int			pro_screen_pcm = 1;		/* 0 = don't use PCM, 1 = use PCM (private color map) */
int			pro_screen_framebuffers = 0;	/* number of DGA framebuffers (1..3 or 0 to auto-detect) */

LOCAL int		pro_screen_open = 0;
LOCAL int		pro_screen_winheight;		/* height of framebuffer */
LOCAL int		pro_screen_bufheight;		/* height of allocated framebuffer */
LOCAL int		pro_screen_updateheight;	/* height to update */

LOCAL int		pro_keyboard_fifo_h;		/* FIFO head pointer */
LOCAL int		pro_keyboard_fifo_t;		/* FIFO tail pointer */
LOCAL int		pro_keyboard_fifo[PRO_KEYBOARD_FIFO_DEPTH];

int			pro_mouse_x = 0;		/* mouse x */
int			pro_mouse_y = 0;		/* mouse y */
int			pro_mouse_l = 0;		/* left mouse button */
int			pro_mouse_m = 0;		/* middle mouse button */
int			pro_mouse_r = 0;		/* right mouse button */
int			pro_mouse_in = 0;		/* mouse in window */

LOCAL Display		*ProDisplay;			/* pointer to the display */
LOCAL int		ProScreen;
LOCAL Window		ProWindow;			/* window structure */
LOCAL GC		ProGC;
LOCAL Colormap		ProColormap;
LOCAL XColor		ProColor_map[8];
LOCAL XColor		ProColor_nonmap[8];
LOCAL int 		ProDepth;			/* color depth */

const long 		ProEventMask = ExposureMask | KeyPressMask
	                               | KeyReleaseMask | PointerMotionMask
	                               | ButtonPressMask | ButtonReleaseMask
	                               | EnterWindowMask | LeaveWindowMask;

LOCAL unsigned long	ProWhitePixel;
LOCAL unsigned long	ProBlackPixel;

#ifdef DGA
#ifdef DGA2
LOCAL int		pro_vid_dga_eventbase;
LOCAL int		pro_vid_dga_errorbase;
#else
LOCAL XF86VidModeModeInfo	pro_modeline;
#endif
LOCAL char		*pro_vid_dga_addr;
LOCAL int		pro_vid_dga_width;
LOCAL int		pro_vid_dga_height;
#ifndef DGA2
LOCAL int		pro_vid_dga_banksize;
#endif
LOCAL int		pro_vid_dga_memsize;
LOCAL Cursor		ProBlankCursor;
LOCAL XColor		ProBlackColor;
LOCAL int		pro_old_scroll;			/* used to determine when to scroll */
LOCAL int		pro_menu_scroll;		/* scroll position when menu is first turned on in DGA mode */
LOCAL int		pro_old_overlay_on;		/* used to determine when overlay has been turned off */
LOCAL int		pro_mouse_y_screen = 0;
#endif

LOCAL XImage		*pro_image;

LOCAL XKeyboardControl	pro_keyboard_control;
LOCAL XKeyboardState	pro_keyboard_state;		/* state to be restored on exit */
LOCAL unsigned char	pro_keyboard_keys[32];		/* key vector */

LOCAL unsigned char	*pro_image_data;
LOCAL int		pro_image_stride;

LOCAL int		pro_screen_clutmode;		/* indicates whether window is in CLUT mode */
LOCAL int		pro_screen_pixsize;		/* bytes per pixel */
LOCAL int		pro_screen_pixsize_act;		/* active bytes per pixel */
LOCAL int		pro_screen_clsize;		/* screen cache line size in bytes */
LOCAL int		pro_screen_blank = 0;		/* indicates whether screen is blanked */

LOCAL int		pro_mask_r;			/* mask for red bits */
LOCAL int		pro_mask_g;			/* mask for green bits */
LOCAL int		pro_mask_b;			/* mask for blue bits */
LOCAL int		pro_nonmap[8];			/* EBO non-mapped colors */
LOCAL int		pro_colormap[8];		/* 8-entry colormap for EBO */
LOCAL int		*pro_lut;			/* points to nonmap or colormap */

#ifdef SHM
LOCAL int		shm_present = 1;		/* indicates whether XShm may be used */

LOCAL XShmSegmentInfo	shminfo;

LOCAL int (*OldXErrorHandler)(Display*, XErrorEvent*);

LOCAL int XShmErrorHandler(Display *dpy, XErrorEvent *xev)
{
  char error[256];

  XGetErrorText(dpy, xev->error_code, error, 255); error[255] = '\0';
  fprintf(stderr, "warning: can't use Shm - %s\r\n", error);
  shm_present = 0;
  return 0;
}
#endif

LOCAL char* current_title = NULL;
LOCAL int title_needs_update = 0;


/* Put a title on the display window */

void pro_screen_title (char *title)
{
          if (current_title)
	    free(current_title);

	  current_title = strdup(title);
          title_needs_update = 1;
}


void pro_screen_clear ()
{
int		i, j, k;
unsigned char	*mptr;


	/* Clear image buffer */

	mptr = pro_image_data;

	for(i=0; i<pro_screen_bufheight;
	    i++, mptr += pro_image_stride*pro_screen_pixsize)
	  for(j=0; j<PRO_VID_SCRWIDTH; j++)
	    for(k=0; k<pro_screen_pixsize; k++)
	      *mptr++ = ProBlackPixel >> (8*k);
}


/* Save and restore keyboard control settings */

void pro_screen_save_old_keyboard ()
{
	XGetKeyboardControl(ProDisplay, &pro_keyboard_state);
}


void pro_screen_restore_old_keyboard ()
{
XKeyboardControl	key_control;


	key_control.bell_pitch = (int)pro_keyboard_state.bell_pitch;
	key_control.bell_duration = (int)pro_keyboard_state.bell_duration;
	key_control.bell_percent = pro_keyboard_state.bell_percent;
	key_control.key_click_percent = pro_keyboard_state.key_click_percent;
	key_control.auto_repeat_mode = pro_keyboard_state.global_auto_repeat;

	XChangeKeyboardControl(ProDisplay, KBAutoRepeatMode | KBKeyClickPercent
	                       | KBBellPitch | KBBellDuration | KBBellPercent,
	                       &key_control);
}


void pro_screen_restore_keyboard ()
{
	if (pro_mouse_in == 1)
	  XChangeKeyboardControl(ProDisplay, KBAutoRepeatMode | KBKeyClickPercent
	                         | KBBellPitch | KBBellDuration | KBBellPercent,
	                         &pro_keyboard_control);
}


/* Keycode lookup routine (key press) */

LOCAL int pro_keyboard_lookup_down (int xkey)
{
	switch (xkey)
	{
	  case XK_Escape:	return PRO_LK201_HOLD;
	  case XK_F1:		return PRO_LK201_PRINT;
	  case XK_F2:		return PRO_LK201_SETUP;
	  case XK_F3:		return PRO_LK201_FFOUR;
	  case XK_F4:		return PRO_LK201_BREAK;
	  case XK_F5:		return PRO_LK201_INT;
	  case XK_F6:		return PRO_LK201_RESUME;
	  case XK_F7:		return PRO_LK201_CANCEL;
	  case XK_F8:		return PRO_LK201_MAIN;
	  case XK_F9:		return PRO_LK201_EXIT;
	  case XK_F10:		return PRO_LK201_ESC;
	  case XK_F11:		return PRO_LK201_BS;
	  case XK_F12:		return PRO_LK201_LF;
	  case XK_Num_Lock: /* XXX temp., since Sys_Req is broken! */
#ifndef XK_Sys_Req
#define XK_Sys_Req 0xFF15
#endif
	  case XK_Sys_Req:	return PRO_LK201_ADDOP; /* XXX seems broken */
	  case XK_Multi_key:	return PRO_LK201_HELP; /* XXX check this! */
	  case XK_Pause:	return PRO_LK201_DO;

	  case XK_a:		return PRO_LK201_A;
	  case XK_b:		return PRO_LK201_B;
	  case XK_c:		return PRO_LK201_C;
	  case XK_d:		return PRO_LK201_D;
	  case XK_e:		return PRO_LK201_E;
	  case XK_f:		return PRO_LK201_F;
	  case XK_g:		return PRO_LK201_G;
	  case XK_h:		return PRO_LK201_H;
	  case XK_i:		return PRO_LK201_I;
	  case XK_j:		return PRO_LK201_J;
	  case XK_k:		return PRO_LK201_K;
	  case XK_l:		return PRO_LK201_L;
	  case XK_m:		return PRO_LK201_M;
	  case XK_n:		return PRO_LK201_N;
	  case XK_o:		return PRO_LK201_O;
	  case XK_p:		return PRO_LK201_P;
	  case XK_q:		return PRO_LK201_Q;
	  case XK_r:		return PRO_LK201_R;
	  case XK_s:		return PRO_LK201_S;
	  case XK_t:		return PRO_LK201_T;
	  case XK_u:		return PRO_LK201_U;
	  case XK_v:		return PRO_LK201_V;
	  case XK_w:		return PRO_LK201_W;
	  case XK_x:		return PRO_LK201_X;
	  case XK_y:		return PRO_LK201_Y;
	  case XK_z:		return PRO_LK201_Z;

	  case XK_0:		return PRO_LK201_0;
	  case XK_1:		return PRO_LK201_1;
	  case XK_2:		return PRO_LK201_2;
	  case XK_3:		return PRO_LK201_3;
	  case XK_4:		return PRO_LK201_4;
	  case XK_5:		return PRO_LK201_5;
	  case XK_6:		return PRO_LK201_6;
	  case XK_7:		return PRO_LK201_7;
	  case XK_8:		return PRO_LK201_8;
	  case XK_9:		return PRO_LK201_9;

	  case XK_minus:	return PRO_LK201_MINUS;
	  case XK_equal:	return PRO_LK201_EQUAL;
	  case XK_bracketleft:	return PRO_LK201_LEFTB;
	  case XK_bracketright:	return PRO_LK201_RIGHTB;

	  case XK_semicolon:	return PRO_LK201_SEMI;
	  case XK_quoteright:	return PRO_LK201_QUOTE;
	  case XK_backslash:	return PRO_LK201_BACKSL;
	  case XK_comma:	return PRO_LK201_COMMA;
	  case XK_period:	return PRO_LK201_PERIOD;
	  case XK_slash:	return PRO_LK201_SLASH;
	  case XK_quoteleft:	return PRO_LK201_TICK;

	  case XK_space:	return PRO_LK201_SPACE;

	  case XK_BackSpace:	return PRO_LK201_DEL;
	  case XK_Return:	return PRO_LK201_RETURN;
	  case XK_Tab:		return PRO_LK201_TAB;

	  case XK_Up:		return PRO_LK201_UP;
	  case XK_Down:		return PRO_LK201_DOWN;
	  case XK_Left:		return PRO_LK201_LEFT;
	  case XK_Right:	return PRO_LK201_RIGHT;

	  case XK_Find:		return PRO_LK201_FIND;
	  case XK_Home:		return PRO_LK201_INSERT;
	  case XK_Prior:	return PRO_LK201_REMOVE;
	  case XK_Delete:	return PRO_LK201_SELECT;
	  case XK_End:		return PRO_LK201_PREV;
	  case XK_Next:		return PRO_LK201_NEXT;

	  case XK_Caps_Lock:	return PRO_LK201_LOCK;
	  case XK_Shift_L:	return PRO_LK201_SHIFT;
	  case XK_Shift_R:	return PRO_LK201_SHIFT;
	  case XK_Control_L:	return PRO_LK201_CTRL;
	  case XK_Control_R:	return PRO_LK201_CTRL;
	  case XK_Alt_L:	return PRO_LK201_COMPOSE;

	  default:		return PRO_NOCHAR;
	}
}


/* Keycode lookup routine (key release) */

LOCAL int pro_keyboard_lookup_up (int xkey)
{
	switch (xkey)
	{
	  case XK_Shift_L:	return PRO_LK201_ALLUPS;
	  case XK_Shift_R:	return PRO_LK201_ALLUPS;
	  case XK_Control_L:	return PRO_LK201_ALLUPS;
	  case XK_Control_R:	return PRO_LK201_ALLUPS;

	  default:		return PRO_NOCHAR;
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

int pro_keyboard_get ()
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

void pro_screen_save_keys ()
{
int		i;
unsigned char	keys[32];


	XQueryKeymap(ProDisplay, keys);

	for(i=0; i<32; i++)
	  pro_keyboard_keys[i] = keys[i];
}


/* Bring emulator state up to date with shift/ctrl key state that may have changed
   while focus was lost */

void pro_screen_update_keys ()
{
int		i, key, oldkey, curkey;
unsigned char	keys[32];


	XQueryKeymap(ProDisplay, keys);

	for(i=0; i<32*8; i++)
	{
	  oldkey = pro_keyboard_keys[i/8]&(1<<(i%8));
	  curkey = keys[i/8]&(1<<(i%8));

	  if (oldkey != curkey) 
	  {
	    key = XKeycodeToKeysym(ProDisplay, i, 0);

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

int pro_screen_init ()
{
XSetWindowAttributes	ProWindowAttributes;	/* structure of window attibutes */
unsigned long		ProWindowMask;		/* mask for window attributes */
XSizeHints		ProSizeHints;		/* window hints for window manger */
int			x, y;			/* window position */
int 			border_width = 0;	/* border width of the window */

XGCValues		ProGCValues;

int			i, index;

unsigned long		planeret[8];
unsigned long		pixret[8];

XEvent			event;
Window			focus_window;
int			revert_to;
#ifdef DGA
int			old_uid = geteuid();
#endif


	if (pro_screen_open == 0)
	{
#ifndef DGA
	  if (pro_screen_full)
	  {
	    printf("Emulator not compiled with DGA support!\r\n");
	    return PRO_FAIL;
	  }
#endif

	  /* Set window coordinates */

	  x = pro_window_x;
	  y = pro_window_y;

	  ProDisplay = XOpenDisplay ("");
	  if (ProDisplay == NULL)
	  {
	    fprintf (stderr, 
	      "ERROR: Could not open a connection to X on display %s\r\n",
	      XDisplayName (NULL));
	    return PRO_FAIL;
	  }

	  ProScreen = DefaultScreen (ProDisplay);

	  ProDepth = DefaultDepth (ProDisplay, ProScreen);

	  ProWhitePixel = WhitePixel(ProDisplay, ProScreen);
	  ProBlackPixel = BlackPixel(ProDisplay, ProScreen);

	  ProWindowAttributes.border_pixel = BlackPixel (ProDisplay, ProScreen);
	  ProWindowAttributes.background_pixel = BlackPixel (ProDisplay, ProScreen);
	  ProWindowAttributes.override_redirect = False;
	  ProWindowAttributes.event_mask = ProEventMask;

	  ProWindowMask = CWEventMask | CWBackPixel | CWBorderPixel;

#ifdef DGA
	  if (pro_screen_full)
	  {
	    int modecount;
#ifdef DGA2
	    XDGADevice	*dgadevice;
	    XDGAMode	*dgamode;
#else
	    int clk;
	    XF86VidModeModeLine	ml;
	    XF86VidModeModeInfo	**modesinfo;
#endif

	    if (seteuid(0) != 0)
	    {
	      printf("DGA error: emulator must be run as root or setuid root\r\n");
	      return PRO_FAIL;
	    }

#ifdef DGA2
	    XDGAOpenFramebuffer(ProDisplay, ProScreen);
	    dgamode = XDGAQueryModes(ProDisplay, ProScreen, &modecount);

	    for(i=0; i<modecount; i++)
	      if ((dgamode[i].viewportWidth == PRO_VID_SCRWIDTH)
	          && (dgamode[i].viewportHeight == pro_screen_full_scale*PRO_VID_SCRHEIGHT))
	        break;
	    XFree(dgamode);

	    if (i == modecount)
	    {
	      printf("DGA2 Error - unable to find correct screen mode\r\n");
	      printf("%d x %d viewport required\r\n", PRO_VID_SCRWIDTH, pro_screen_full_scale*PRO_VID_SCRHEIGHT);
	      seteuid(old_uid);
	      return PRO_FAIL;
	    }

	    dgadevice = XDGASetMode(ProDisplay, ProScreen, i+1);

            pro_vid_dga_addr = dgadevice -> data;
            pro_vid_dga_width = (dgadevice -> mode).imageWidth;
            pro_vid_dga_height = (dgadevice -> mode).imageHeight;

	    if (pro_nine_workaround)
	    {
	      /* This is a workaround for a #9 DGA2 bug that reports
	         framebuffer height incorrectly as that of a single screen */

	      if (pro_vid_dga_height <= 1024)
	        pro_vid_dga_height = 2304;
	    }

            pro_vid_dga_memsize = pro_vid_dga_height * (dgadevice -> mode).bytesPerScanline / 1024;
	    XFree(dgadevice);

#ifdef DGAXXX
	    /* XXX Does not seem to pass DGA events ???  Use XEvents for now... */

	    XDGASelectInput(ProDisplay, ProScreen, ProEventMask);
#endif
	    XDGAQueryExtension(ProDisplay, &pro_vid_dga_eventbase, &pro_vid_dga_errorbase);

#else /* DGA1 */

	    /* Get current screen mode */

	    XF86VidModeGetModeLine(ProDisplay, ProScreen, &clk, &ml);
	    if (ml.privsize != 0)
	      XFree(ml.private);

	    /* Attempt to find appropriate screen mode */

	    XF86VidModeGetAllModeLines(ProDisplay, ProScreen, &modecount, &modesinfo);

	    for(i=0; i<modecount; i++)
	      if ((modesinfo[i]->hdisplay == PRO_VID_SCRWIDTH)
	          && (modesinfo[i]->vdisplay == pro_screen_full_scale*PRO_VID_SCRHEIGHT))
	        break;

	    if (i == modecount)
	    {
	      printf("DGA1 Error - unable to find correct screen mode\r\n");
	      printf("%d x %d viewport required\r\n", PRO_VID_SCRWIDTH, pro_screen_full_scale*PRO_VID_SCRHEIGHT);
	      seteuid(old_uid);
	      return PRO_FAIL;
	    }

	    /* Save current screen mode */

	    memset(&pro_modeline, 0, sizeof(pro_modeline));

	    pro_modeline.dotclock = clk;
	    pro_modeline.hdisplay = ml.hdisplay;
	    pro_modeline.hsyncstart = ml.hsyncstart;
	    pro_modeline.hsyncend = ml.hsyncend;
	    pro_modeline.htotal = ml.htotal;
	    pro_modeline.vdisplay = ml.vdisplay;
	    pro_modeline.vsyncstart = ml.vsyncstart;
	    pro_modeline.vsyncend = ml.vsyncend;
	    pro_modeline.vtotal = ml.vtotal;
	    pro_modeline.flags = ml.flags;

	    /* XXX Blank screen here */

	    /* Switch screen mode */

	    XF86VidModeSwitchToMode(ProDisplay, ProScreen, modesinfo[i]);

	    /* XXX is this correct? */

	    for(i=0; i<modecount; i++)
	    if (modesinfo[i]->privsize != 0)
	      XFree(modesinfo[i]->private);

	    XFree(modesinfo); /* XXX correct? */

	    /* Get frame buffer parameters */
	  
            XF86DGAGetVideo(ProDisplay, ProScreen, 
	                    &pro_vid_dga_addr, 
	                    &pro_vid_dga_width,
	                    &pro_vid_dga_banksize,
	                    &pro_vid_dga_memsize);

	    /* Enter DGA mode */

	    XF86DGADirectVideo(ProDisplay, ProScreen,
	                       XF86DGADirectGraphics
	                       | XF86DGADirectMouse
	                       | XF86DGADirectKeyb);
#endif

	    seteuid(old_uid);

	    x = 0;
	    y = 0;
	  }
#endif

	  /* Determine pixel depth */
/* XXX
	  printf("%d bpp\r\n", ProDepth);
*/

	  /* XXX use better technique to detect CLUT modes */

	  switch(ProDepth)
	  {
	    case 8:
	      pro_screen_clutmode = 1;
	      pro_screen_pixsize = 1;
	      pro_screen_pixsize_act = 1;
	      break;

	    case 12:
	      pro_screen_clutmode = 1;
	      pro_screen_pixsize = 2;
	      pro_screen_pixsize_act = 2;
	      break;

	    case 15:
	    case 16:
	      pro_screen_clutmode = 0;
	      pro_screen_pixsize = 2;
	      pro_screen_pixsize_act = 2;
	      break;

	    case 24:
	      pro_screen_clutmode = 0;
	      pro_screen_pixsize = 4;
	      pro_screen_pixsize_act = 3;
	      break;

	    default:
	      printf("Unsupported pixel depth\r\n");
	      return PRO_FAIL;
	  }

#ifdef DGA
	  if (pro_screen_full)
	  {
	    /* Check if number of framebuffers should be auto-detected */

	    if (pro_screen_framebuffers == 0)
	    {
	      pro_screen_framebuffers = (pro_vid_dga_memsize*1024) /
	        (pro_vid_dga_width*pro_screen_pixsize*pro_screen_full_scale*PRO_VID_MEMHEIGHT);

	      if (pro_screen_framebuffers > 3)
	        pro_screen_framebuffers = 3;

	      if (pro_screen_framebuffers < 1)
	        pro_screen_framebuffers = 1;

	      printf("Auto-detected %d framebuffer", pro_screen_framebuffers);
	      if (pro_screen_framebuffers > 1)
	        printf("s");
	      printf("\r\n");
	    }

	    pro_screen_bufheight = pro_screen_framebuffers*pro_screen_full_scale*PRO_VID_MEMHEIGHT;
	    pro_screen_winheight = pro_screen_bufheight;
	    pro_screen_updateheight = PRO_VID_MEMHEIGHT;
	  }
	  else
#endif
	  {
	    pro_screen_bufheight = PRO_VID_SCRHEIGHT;
	    pro_screen_winheight = pro_screen_window_scale*pro_screen_bufheight;
	    pro_screen_updateheight = PRO_VID_SCRHEIGHT;
	  }
	  

	  ProWindow = XCreateWindow (ProDisplay, 
				RootWindow (ProDisplay, ProScreen),
				x, y, PRO_VID_SCRWIDTH, pro_screen_winheight, border_width,
				ProDepth, InputOutput, CopyFromParent,
				ProWindowMask, &ProWindowAttributes);

	  ProSizeHints.flags = PSize | PMinSize | PMaxSize;

	  if ((x != 0) && (y != 0))
	  {
	    ProSizeHints.flags |= PPosition;
	    ProSizeHints.x = x;
	    ProSizeHints.y = y;
	  }

	  ProSizeHints.width = ProSizeHints.min_width = ProSizeHints.max_width
	    = PRO_VID_SCRWIDTH;
	  ProSizeHints.height = ProSizeHints.min_height = ProSizeHints.max_height
	    =  pro_screen_winheight;

	  XSetNormalHints (ProDisplay, ProWindow, &ProSizeHints);

	  ProGC = XCreateGC (ProDisplay,
                             ProWindow,
                             (unsigned long) 0,
                             &ProGCValues);

	  /* error... cannot create gc */
	  if (ProGC == 0)
	  {
	    XDestroyWindow(ProDisplay, ProScreen);
	    return PRO_FAIL;
	  }

	  XSetForeground (ProDisplay, ProGC, ProBlackPixel);
	  XSetBackground (ProDisplay, ProGC, ProWhitePixel);


	  XMapWindow (ProDisplay, ProWindow);

	  /* Wait for window to become visible */
	  /* XXX #9 server sometimes hangs here with no events */

#ifdef DGA
#ifdef DGA2
	  /* XXX Major #9 hack to account for missing Expose/EnterNotify
	     events in DGA mode */

	  if (!pro_screen_full || !pro_nine_workaround)
#endif
#endif
	  do
	    XNextEvent(ProDisplay, &event);
	  while ((event.type != Expose) && (event.type != EnterNotify));

	  /* Determine whether window has input focus */

#ifdef DGA
	  if (pro_screen_full)
	    pro_mouse_in = 1;
	  else
#endif
	  {
	    XGetInputFocus (ProDisplay, &focus_window, &revert_to);

	    if (focus_window == ProWindow)
	      pro_mouse_in = 1;
	    else
	      pro_mouse_in = 0;
	  }

	  pro_screen_title("Pro 350");

	  pro_screen_clsize = PRO_VID_CLS_PIX * pro_screen_pixsize;

	  if ((pro_screen_pcm == 1) && (pro_screen_clutmode == 1))

	    /* Allocate private colormap */

	    ProColormap = XCreateColormap(ProDisplay,
				ProWindow, 
				DefaultVisual(ProDisplay, DefaultScreen(ProDisplay)),
				AllocNone);
	  else
	    /* Use default colormap */

	    ProColormap = DefaultColormap(ProDisplay, ProScreen);

	  /* Allocate colors */

	  /* XXX also, alloc only 2 colors if non-ebo */

	  if (pro_screen_clutmode == 1)
	    XAllocColorCells(ProDisplay, ProColormap, True, planeret, 0, pixret, 8);

	  for(i=0; i<8; i++)
	  {
	    if (pro_screen_clutmode == 1)
	    {
	      index = pixret[i];
	      ProColor_nonmap[i].pixel = index;
	      ProColor_map[i].pixel = index;

	      /* Determine whether PRO is in colormapped mode */

	      if ((pro_vid_csr & PRO_VID_CME) != 0)
	        XStoreColor(ProDisplay, ProColormap, &ProColor_map[i]);
	      else
	        XStoreColor(ProDisplay, ProColormap, &ProColor_nonmap[i]);

	      pro_colormap[i] = index;
	      pro_nonmap[i] = index;
	    }
	    else
	    {
	      XAllocColor(ProDisplay, ProColormap, &ProColor_nonmap[i]);
	      pro_nonmap[i] = ProColor_nonmap[i].pixel;

	      XAllocColor(ProDisplay, ProColormap, &ProColor_map[i]);
	      pro_colormap[i] = ProColor_map[i].pixel;
	    }
	  }


	  /* Assign RGB mask values (for overlay blending) */

	  if (pro_screen_clutmode == 1)
	  {
	    pro_mask_r = ~0;
	    pro_mask_g = 0;
	    pro_mask_b = 0;
	  }
	  else
	  {
	    pro_mask_r = pro_nonmap[1];
	    pro_mask_g = pro_nonmap[2];
	    pro_mask_b = pro_nonmap[4];
	  }

	  XSetWindowColormap(ProDisplay, ProWindow, ProColormap);

#if !PRO_EBO_PRESENT
	  pro_nonmap[1] = pro_nonmap[7]; /* XXX change this to 1 */
#endif

	  /* Create image buffer for screen updates */
#ifdef DGA
	  if (pro_screen_full)
	  {
	    pro_image_data = pro_vid_dga_addr;
	    pro_image_stride = pro_vid_dga_width - PRO_VID_SCRWIDTH;
	  }
	  else
#endif
	  {
	    pro_image_stride = 0;
#ifdef SHM
	    if (XShmQueryExtension(ProDisplay))
	    {
	      /* Use shared memory */

	      OldXErrorHandler = XSetErrorHandler(XShmErrorHandler);

	      memset(&shminfo, 0, sizeof(shminfo));
	      if (!(pro_image = XShmCreateImage(ProDisplay,
					      DefaultVisual(ProDisplay, DefaultScreen(ProDisplay)),
					      ProDepth,
					      ZPixmap,
					      NULL,
					      &shminfo,
					      PRO_VID_SCRWIDTH, PRO_VID_SCRHEIGHT)))
	        shm_present = 0;
	      else if ((shminfo.shmid = 
		        shmget(IPC_PRIVATE, pro_image->bytes_per_line*PRO_VID_SCRHEIGHT, IPC_CREAT|0777))==-1)
	        shm_present = 0;
	      else if ((shminfo.shmaddr = pro_image->data = shmat(shminfo.shmid, 0, 0))==(char*)-1)
	        shm_present = 0;
	      else
	      {
	        if (XShmAttach(ProDisplay, &shminfo)==0) shm_present = 0;
	        XSync(ProDisplay, True);
	        shmctl(shminfo.shmid, IPC_RMID, 0);

	        if (!shm_present) shmdt(shminfo.shmaddr);
	      }
	      if (pro_image && !shm_present) XDestroyImage(pro_image);
	      XSync(ProDisplay, True);
	      XSetErrorHandler(OldXErrorHandler);
	    }
	    else shm_present = 0;

	    if (shm_present != 0)
	      pro_image_data = pro_image->data;
	    else
#endif /* SHM */
	    {
	      /* Use non-shared memory */

	      pro_image_data = (unsigned char *)malloc(PRO_VID_SCRWIDTH*PRO_VID_SCRHEIGHT*pro_screen_pixsize);

	      pro_image = XCreateImage(ProDisplay,
			  DefaultVisual(ProDisplay, DefaultScreen(ProDisplay)),
			  ProDepth,
			  ZPixmap,
			  0,
			  (char*) pro_image_data,
			  PRO_VID_SCRWIDTH, PRO_VID_SCRHEIGHT,
			  pro_screen_pixsize*8,
			  PRO_VID_CLS_PIX*pro_screen_pixsize);
	    }
	  }

	  /* Save old keyboard state */

	  pro_screen_save_old_keyboard ();

	  /* Restore emulator keyboard state */

	  pro_screen_restore_keyboard();

	  XFlush (ProDisplay);

	  /* Make emulator state consistent with keyboard state */

	  pro_screen_update_keys();


	  /* Initialize the overlay frame buffer */

	  pro_overlay_init(pro_screen_pixsize, pro_screen_clutmode,
	                   (int)ProBlackPixel, (int)ProWhitePixel);

	  pro_screen_open = 1;

#ifdef DGA
	  if (pro_screen_full)
	  {
	    /* Check if enough video memory */

	    if (pro_screen_winheight*pro_vid_dga_width*pro_screen_pixsize
	        >  pro_vid_dga_memsize*1024)
	    {
	      printf("DGA error: not enough video memory! (%d required, only %d present)\r\n",
	              pro_screen_winheight*pro_vid_dga_width*pro_screen_pixsize,
	              pro_vid_dga_memsize*1024);
	      return PRO_FAIL;
	    }

#ifdef DGA2
	    XDGASetViewport(ProDisplay, ProScreen, 0, 0, XDGAFlipImmediate);
#else
	    XF86DGASetViewPort(ProDisplay, ProScreen, 0, 0);
#endif

	    /* Turn off X cursor */

	    ProBlackColor.red = 0;
	    ProBlackColor.green = 0;
	    ProBlackColor.blue = 0;

	    ProBlankCursor = XCreatePixmapCursor(ProDisplay,
				  XCreatePixmap(ProDisplay, ProWindow, 1, 1, 1),
				  XCreatePixmap(ProDisplay, ProWindow, 1, 1, 1),
				  &ProBlackColor, &ProBlackColor, 0 ,0);

	    XDefineCursor(ProDisplay, ProWindow, ProBlankCursor);

#ifdef DGA2
	    if (pro_screen_full)
	      XDGASync(ProDisplay, ProScreen); /* XXX not sure if needed */

	    /* XXX Keyboard grabbing sometimes fails without this
	       on #9 server */

	    if (pro_nine_workaround)
	      sleep(1);
#endif
	    /* Grab Keyboard */

	    if (XGrabKeyboard(ProDisplay, ProWindow, True, GrabModeAsync,
	        GrabModeAsync, CurrentTime) != GrabSuccess)
	    {
	      printf("DGA error: unable to grab keyboard\r\n");
	      return PRO_FAIL;
	    }

	    /* Grab Mouse */

	    if (XGrabPointer(ProDisplay, ProWindow, True,
	        PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
	        GrabModeAsync, GrabModeAsync, None, None,
	        CurrentTime) != GrabSuccess)
	    {
	      printf("DGA error: unable to grab mouse\r\n");
	      return PRO_FAIL;
	    }

	    pro_old_scroll = -2;
	    pro_old_overlay_on = 0;

	    /* XXX Unblank screen here */
	  }
#endif
	  /* Clear image buffer */

	  pro_screen_clear();

	  /* Invalidate display cache */

	  pro_clear_mvalid();
	}

	return PRO_SUCCESS;
}


void pro_screen_close ()
{
/* XXX not currently used - see below
XWindowAttributes	winattr;
*/


	if (pro_libc_workaround)
	  printf("1 DDD\r\n");

	if (pro_screen_open)
	{
	  if (pro_libc_workaround)
	    printf("2 DDD\r\n");

	  pro_screen_open = 0;

	  /* XXX deallocate colors? */

	  /* Restore keyboard parameters */

	  if (pro_libc_workaround)
	    printf("3 DDD\r\n");

	  if (pro_mouse_in == 1)
	    pro_screen_restore_old_keyboard();

	  if (pro_libc_workaround)
	    printf("4 DDD\r\n");

	  /* Save keymap */

	  pro_screen_save_keys();

	  if (pro_libc_workaround)
	    printf("5 DDD\r\n");


	  /* Close screen */

#ifdef DGA
	  if (pro_screen_full)
	  {
	    /* XXX Blank screen here */

	    if (pro_libc_workaround)
	      printf("6 DDD\r\n");

	    XUngrabKeyboard(ProDisplay, CurrentTime);

	    if (pro_libc_workaround)
	      printf("7 DDD\r\n");

	    XUngrabPointer(ProDisplay, CurrentTime);

	    if (pro_libc_workaround)
	      printf("8 DDD\r\n");

#ifdef DGA2
	    XDGACloseFramebuffer(ProDisplay, ProScreen);

	    /* XXX Major #9 hack to return screen to proper mode */

	    if (pro_nine_workaround)
	      XDGASetMode(ProDisplay, ProScreen, 1);

	    XDGASetMode(ProDisplay, ProScreen, 0);
#else
	    XF86DGADirectVideo(ProDisplay, ProScreen, 0);

	    if (pro_libc_workaround)
	      printf("9 DDD\r\n");

	    XF86VidModeSwitchToMode(ProDisplay, ProScreen, &pro_modeline);

	    if (pro_libc_workaround)
	      printf("10 DDD\r\n");
#endif
	  }
	  else
#endif
	  {
#ifdef SHM
            if (shm_present)
            {
	      if (pro_libc_workaround)
	        printf("11 DDD\r\n");

	      XShmDetach(ProDisplay, &shminfo);

	      if (pro_libc_workaround)
	        printf("12 DDD\r\n");

	      XDestroyImage(pro_image);

	      if (pro_libc_workaround)
	        printf("13 DDD\r\n");

	      shmdt(shminfo.shmaddr);

	      if (pro_libc_workaround)
	        printf("14 DDD\r\n");
            }
            else
#endif
	      XDestroyImage(pro_image);
	  }

	  /* Save window x, y coordinates */

/* XXX broken - always returns (0,0)
	  XGetWindowAttributes (ProDisplay, ProWindow, &winattr);

	  pro_window_x = winattr.x;
	  pro_window_y = winattr.y;
*/

	  if (pro_libc_workaround)
	    printf("15 DDD\r\n");

	  XFreeGC (ProDisplay, ProGC);

	  if (pro_libc_workaround)
	    printf("16 DDD\r\n");

	  XDestroyWindow (ProDisplay, ProWindow);

	  if (pro_libc_workaround)
	    printf("17 DDD\r\n");

	  XCloseDisplay (ProDisplay);

	  if (pro_libc_workaround)
	    printf("18 DDD\r\n");

	  /* Close the overlay frame buffer */

	  pro_overlay_close();

	  if (pro_libc_workaround)
	    printf("19 DDD\r\n");
	}

	/* XXX Unblank screen here */
}


/* Reset routine (called only once) */

void pro_screen_reset ()
{
int	r, g, b, i;


	/* Clear keymap */

	memset(&pro_keyboard_keys, 0, sizeof(pro_keyboard_keys));

	/* Reset keyboard control parameters */

	pro_keyboard_control.bell_pitch = PRO_LK201_BELL_PITCH;
	pro_keyboard_control.bell_duration = PRO_LK201_BELL_DURATION;
	pro_keyboard_control.bell_percent = 100;
	pro_keyboard_control.key_click_percent = 0;
	pro_keyboard_control.auto_repeat_mode = AutoRepeatModeOn;

	/* Initialize colormaps */

	for(r=0; r<=1; r++)
	  for(g=0; g<=1; g++)
	    for(b=0; b<=1; b++)
	    {
	      i = (r | (g<<1) | (b<<2));

	      ProColor_nonmap[i].red = r * 65535;
	      ProColor_nonmap[i].green = g * 65535;
	      ProColor_nonmap[i].blue = b * 65535;
	      ProColor_nonmap[i].flags = DoRed|DoGreen|DoBlue;

	      ProColor_map[i].red = 0;
	      ProColor_map[i].green = 0;
	      ProColor_map[i].blue = 0;
	      ProColor_map[i].flags = DoRed|DoGreen|DoBlue;
	    }
}


/* This function is called whenever the colormap mode changes.
   A new X11 colormap is loaded for private colormap modes,
   mvalid is cleared otherwise */

void pro_mapchange ()
{
XColor	*ProColor;

	if (pro_screen_clutmode == 1)
	{
	  if ((pro_vid_csr & PRO_VID_CME) != 0)
	    ProColor = ProColor_map;
	  else
	    ProColor = ProColor_nonmap;

	  /* XXX only 2 colors for non-ebo */

	  XStoreColors(ProDisplay, ProColormap, ProColor, 8);
	}
	else
	  pro_clear_mvalid();
}

/* This writes an 8-bit (3-3-2) RGB value into the PRO's colormap */

void pro_colormap_write (int index, int rgb)
{
unsigned long	pixel;
double		r, g, b;

#if PRO_EBO_PRESENT
	/* Calculate linear color component values 0.0 - 1.0 */

	r = (double)((rgb & PRO_VID_R_MASK) >> 5)/7.0;
	g = (double)((rgb & PRO_VID_G_MASK) >> 2)/7.0;
	b = (double)(rgb & PRO_VID_B_MASK)/3.0;

	/* Perform gamma correction */

	r = pow(r, 10.0/(double)pro_screen_gamma);
	g = pow(g, 10.0/(double)pro_screen_gamma);
	b = pow(b, 10.0/(double)pro_screen_gamma);

	ProColor_map[index].red = (int)(r * 65535.0);
	ProColor_map[index].green = (int)(g * 65535.0);
	ProColor_map[index].blue = (int)(b * 65535.0);

	if (pro_screen_clutmode == 0)
	{
	  /* Free old color */

	  pixel = pro_colormap[index];

	  XFreeColors(ProDisplay, ProColormap, &pixel, 1, 0L);

	  /* Allocate new color */

	  XAllocColor(ProDisplay, ProColormap, &ProColor_map[index]);

	  pro_colormap[index] = ProColor_map[index].pixel;
	}

	/* Check if in colormapped mode */

	if ((pro_vid_csr & PRO_VID_CME) != 0)
	{
	  if (pro_screen_clutmode == 1)
	  {
	    XStoreColor(ProDisplay, ProColormap, &ProColor_map[index]);
	  }
	  else
	  {
	    /* The cache gets cleared only for non-private colormap X11 modes */

	    pro_clear_mvalid();
	  }
	}
#endif

/* XXX
	printf("Colormap write: index = %x rgb = %x colormap = %x\r\n", index, rgb, pro_colormap[index]);
*/
}


/* This is called whenever the scroll register changes */

void pro_scroll ()
{
	/* Clear entire display cache if DGA disabled,
	   or if DGA is using only a single framebuffer */

#ifdef DGA
	if ((pro_screen_full == 0) || (pro_screen_framebuffers == 1))
#endif
	  pro_clear_mvalid();
}


/* Blending functions used for overlays */

#define blend \
  a = pro_overlay_alpha[pnum]; \
  na = PRO_OVERLAY_MAXA - a; \
  color = (((na*(pro_mask_r&color) + a*(pro_mask_r&opix))/PRO_OVERLAY_MAXA)&pro_mask_r) \
          + (((na*(pro_mask_g&color) + a*(pro_mask_g&opix))/PRO_OVERLAY_MAXA)&pro_mask_g) \
          + (((na*(pro_mask_b&color) + a*(pro_mask_b&opix))/PRO_OVERLAY_MAXA)&pro_mask_b)


/* This is called every emulated vertical retrace */

void pro_screen_update ()
{
int		i, vdata0, vdata1, vdata2, vindex, vpix, vpixs, vpixe, x, y;
int		a, na, opix, pnum, color, cindex;
int		offset, reps;
#ifdef DGA
int		j, cur_scroll, scroll;
#endif
unsigned char	*image_data;


	/* Service X events */

	pro_screen_service_events();

	image_data = pro_image_data;

#ifdef DGA
	scroll = -1; /* viewport will be updated if > -1 */

	if (pro_screen_full && (pro_screen_framebuffers > 1))
	{
	  /* Check if scroll register has changed */
	  /* (only cause screen scroll if overlay is off) */

	  cur_scroll = pro_vid_scl & 0xff;

	  offset = (PRO_VID_SCRWIDTH+pro_image_stride)*pro_screen_pixsize
	           *pro_screen_winheight/pro_screen_framebuffers;

	  reps = 2;

	  if (pro_overlay_on == 0)
	  {
	    vindex = 0;

	    /* Restore the viewport position if overlay was just turned off */
	    /* Or scroll viewport if scroll register has changed */

	    if ((pro_old_overlay_on != 0) || (cur_scroll != pro_old_scroll))
	      scroll = cur_scroll;
	  }
	  else
	  {
	    vindex = vmem(0);

	    /* Reset the viewport position if overlay was just turned on */
	    /* And force full screen redraw */

	    if (pro_old_overlay_on == 0)
	    {
	      pro_clear_mvalid();

	      if (pro_screen_framebuffers == 3)
	        scroll = 2*PRO_VID_MEMHEIGHT;
	      else
	        scroll = pro_menu_scroll = cur_scroll;
	    }

	    /* If the overlay is on, scroll the screen by redrawing */

	    if (cur_scroll != pro_old_scroll)
	      pro_clear_mvalid();

	    /* Use third frame buffer for overlay mode (if enabled) */
	    /* Otherwise, align frame buffer with visible portion of
	       screen when menu was turned on */

	    if (pro_screen_framebuffers == 3)
	      image_data += 2*offset;
	    else
	      image_data += (PRO_VID_SCRWIDTH+pro_image_stride)
	                    *pro_screen_pixsize*pro_screen_full_scale
	                    *pro_menu_scroll;
	  }

	  pro_old_scroll = cur_scroll;
	  pro_old_overlay_on = pro_overlay_on;
	}
	else
#endif
	{
	  offset = 0;
	  reps = 1;
	  vindex = vmem(0);
	}

	/* Check whether screen has been blanked */

	if ((pro_vid_p1c & PRO_VID_P1_HRS) == PRO_VID_P1_OFF)
	{
	  if (pro_screen_blank == 0)
	  {
	    /* Blank the screen */

#ifdef DGA
	    if (pro_screen_full)
	      pro_screen_clear();
	    else
#endif
	      XClearWindow(ProDisplay, ProWindow);

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
	    pro_lut = pro_colormap;
	  else
	    pro_lut = pro_nonmap;
#else
	  pro_lut = pro_nonmap;
#endif

	  /* Redraw portions of screen that have changed */

	  for(y=0; y<pro_screen_updateheight; y++)
	  {
	    for(x=0; x<PRO_VID_SCRWIDTH; x+=PRO_VID_CLS_PIX)
	    {
	      /* Update screen segment only if display cache is invalid */

	      if (pro_vid_mvalid[cmem(vindex)] == 0)
	      {
	      if (pro_overlay_on == 0)
#ifdef DGA
	        for(j = 1; j >= (3-2*reps); image_data += j*offset, j -= 2)
	        {
#endif
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
	              color = pro_lut[cindex];
  
	              switch(pro_screen_pixsize)
	              {
	                case 1:
	                  image_data[vpix] = color;
	                  break;
	                case 2:
	                  ((unsigned short *)image_data)[vpix] = color;
	                  break;
	                case 4:
	                  ((unsigned int *)image_data)[vpix] = color;
	                  break;
	                default:
	                  break;
	              }

	              vdata0 = vdata0 >> 1;
	              vdata1 = vdata1 >> 1;
	              vdata2 = vdata2 >> 1;
	            }
	          }
#ifdef DGA
	        }
#endif
	        else
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
	              color = pro_lut[cindex];
  
	              pnum = y*PRO_VID_SCRWIDTH+x+vpix;

	              switch(pro_screen_pixsize)
	              {
	                case 1:
	                  opix = pro_overlay_data[pnum];
	                  blend;
	                  image_data[vpix] = color;
	                  break;
	                case 2:
	                  opix = ((unsigned short *)pro_overlay_data)[pnum];
	                  blend;
	                  ((unsigned short *)image_data)[vpix] = color;
	                  break;
	                case 4:
	                  opix = ((unsigned int *)pro_overlay_data)[pnum];
	                  blend;
	                  ((unsigned int *)image_data)[vpix] = color;
	                  break;
	                default:
	                  break;
	              }

	              vdata0 = vdata0 >> 1;
	              vdata1 = vdata1 >> 1;
	              vdata2 = vdata2 >> 1;
	            }
	          }

	        /* Draw cache-line on screen */
#ifdef DGA
	        if (pro_screen_full == 0)
#endif
	        {
#ifdef SHM
		  if (shm_present)
		    XShmPutImage(ProDisplay, ProWindow, ProGC, pro_image, x, y,
		                 x, y*pro_screen_window_scale, PRO_VID_CLS_PIX, 1, False);
		  else
#endif
		    XPutImage(ProDisplay, ProWindow, ProGC, pro_image, x, y,
		              x, y*pro_screen_window_scale, PRO_VID_CLS_PIX, 1);
	        }

	        /* Mark cache entry valid */

	        pro_vid_mvalid[cmem(vindex)] = 1;
	      }

	      vindex = (vindex + (PRO_VID_CLS_PIX/16)) & PRO_VID_VADDR_MASK;

	      image_data += pro_screen_clsize;
	    }
#ifdef DGA
	    if (pro_screen_full)
	      image_data += ((pro_screen_full_scale-1)*PRO_VID_SCRWIDTH+pro_screen_full_scale*pro_image_stride)*pro_screen_pixsize;
#endif
	  }

	  XFlush (ProDisplay);
	}

	/* Check if title needs to be updated */

	if (title_needs_update)
	{
	  title_needs_update = 0;
#ifdef DGA
/* XXX
	  if (pro_screen_full)
	    printf("%s\r\n", current_title);
*/
#endif
	  XStoreName(ProDisplay, ProWindow, current_title);
	  XFlush(ProDisplay);
	}

#ifdef DGA
	/* Move viewport, if required */

	if (pro_screen_full)
	{
	  if (scroll >= 0)
#ifdef DGA2
	  {
	    XDGASetViewport(ProDisplay, ProScreen, 0, pro_screen_full_scale*scroll, XDGAFlipImmediate);

	    /* Wait for viewport update in case XDGAFlipImmediate is not
	       supported */

	    while(XDGAGetViewportStatus(ProDisplay, ProScreen));
	  }
#else
	    XF86DGASetViewPort(ProDisplay, ProScreen, 0, pro_screen_full_scale*scroll);
#endif
	}
#endif
}


/* Set keyboard bell volume */

void pro_keyboard_bell_vol (int vol)
{
	/* XXX This actually seems to change the duration under Linux!? */

	/* vol is in the range 0 (loudest) to 7 (softest) */

	pro_keyboard_control.bell_percent = (100*(7-vol))/7;

	pro_screen_restore_keyboard();

	XFlush(ProDisplay);
}


/* Sound keyboard bell */

void pro_keyboard_bell ()
{
	XBell(ProDisplay, 0);
	XFlush(ProDisplay);
}


/* Turn off auto-repeat */

void pro_keyboard_auto_off ()
{
	pro_keyboard_control.auto_repeat_mode = AutoRepeatModeOff;

	pro_screen_restore_keyboard();

	XFlush(ProDisplay);
}


/* Turn on auto-repeat */

void pro_keyboard_auto_on ()
{
	pro_keyboard_control.auto_repeat_mode = AutoRepeatModeOn;

	pro_screen_restore_keyboard();

	XFlush(ProDisplay);
}


/* Turn off keyclick */

void pro_keyboard_click_off ()
{
	pro_keyboard_control.key_click_percent = 0;

	pro_screen_restore_keyboard();

	XFlush(ProDisplay);
}


/* Turn on keyclick */

void pro_keyboard_click_on ()
{
	pro_keyboard_control.key_click_percent = 100;

	pro_screen_restore_keyboard();

	XFlush(ProDisplay);
}


/* The following is a workaround for an x11-hang bug under Linux */

int XCheckMaskEvent_nohang(Display* dpy, long event_mask, XEvent* event)
{
   static int checkready = 0;
   static sigset_t checkmask;
   sigset_t oldmask;
   Bool event_return;

   if (!checkready)
   {
      checkready = 1;
      sigemptyset(&checkmask);
/*
      sigaddset(&checkmask, SIGUSR1);
      sigaddset(&checkmask, SIGUSR2);
*/
      sigaddset(&checkmask, SIGALRM);
      /*
       * Add any other signals to the mask that might otherwise
       * be set off during the XCheckMaskEvent.  It is possible
       * that none of this is needed if libX11 is compiled with
       * linux libc-6, because a lot of the code in libX11 compiles
       * differently with libc-6 (because of improved thread support.)
       * Meanwhile, this hack should allow it to work with libc-5.
       */
   }
   sigprocmask(SIG_BLOCK, &checkmask, &oldmask);
   event_return = XCheckMaskEvent(dpy, event_mask, event);
   sigprocmask(SIG_UNBLOCK, &oldmask, 0);
   return event_return;
}


/* Service X events */

void pro_screen_service_events ()
{
XEvent		event; /* XXX should this be XDGAEvent for DGA2? */
#ifdef DGAXXX
XKeyEvent	keyevent;
#endif
int		expose = 0;
int		key;
int		eventtype;


	/* If multiple events are buffered up, flush them all out */

	/* XXX bug workaround */

	while (XCheckMaskEvent_nohang(ProDisplay, ProEventMask, (XEvent *)&event))
	{
	  eventtype = event.type;

#ifdef DGAXXX
	  if (pro_screen_full)
	    eventtype -= pro_vid_dga_eventbase;
#endif
	
	  switch (eventtype)
	  {
	    case Expose:
	      expose = 1;
	      break;

	    case KeyPress:
#ifdef DGAXXX
#ifdef DGA
#ifdef DGA2
	      if (pro_screen_full)
	      {
	        XDGAKeyEventToXKeyEvent((XDGAKeyEvent*)&event.xkey, (XKeyEvent*)&keyevent);
	        key = XLookupKeysym((XKeyEvent*)&keyevent, 0);
	      }
              else
#endif
#endif
#endif
	      key = XLookupKeysym((XKeyEvent*)&event, 0);
	      key = pro_keyboard_lookup_down(key);
	      if (key != PRO_NOCHAR)
	        pro_keyboard_fifo_put(key);
	      break;

	    case KeyRelease:
#ifdef DGAXXX
#ifdef DGA
#ifdef DGA2
	      if (pro_screen_full)
	      {
	        XDGAKeyEventToXKeyEvent((XDGAKeyEvent*)&event.xkey, (XKeyEvent*)&keyevent);
	        key = XLookupKeysym((XKeyEvent*)&keyevent, 0);
	      }
              else
#endif
#endif
#endif
	      key = XLookupKeysym((XKeyEvent*)&event, 0);
	      key = pro_keyboard_lookup_up(key);
	      if (key != PRO_NOCHAR)
	        pro_keyboard_fifo_put(key);
	      break;

	    case MotionNotify:
#ifdef DGA
	      if (pro_screen_full)
	      {
	        /* DGA mouse events are returned as *relative* motion */

	        pro_mouse_x += ((XMotionEvent*)&event)->x_root;
	        pro_mouse_y_screen += ((XMotionEvent*)&event)->y_root;
	        pro_mouse_y = pro_mouse_y_screen/pro_screen_full_scale;
	        if (pro_mouse_x >= PRO_VID_SCRWIDTH)
	          pro_mouse_x = PRO_VID_SCRWIDTH-1;
	        if (pro_mouse_x < 0)
	          pro_mouse_x = 0;
	        if (pro_mouse_y >= PRO_VID_SCRHEIGHT)
	          pro_mouse_y = PRO_VID_SCRHEIGHT-1;
	        if (pro_mouse_y < 0)
	          pro_mouse_y = 0;
	      }
	      else
#endif
	      {
	        pro_mouse_x = ((XMotionEvent*)&event)->x;
	        pro_mouse_y = (((XMotionEvent*)&event)->y)/pro_screen_window_scale;
	      }

	      break;

	    case ButtonPress:
	      switch (((XButtonEvent *)&event)->button)
	      {
	        case Button1:
	          pro_mouse_l = 1;
	          break;

	        case Button2:
	          pro_mouse_m = 1;
	          break;

	        case Button3:
	          pro_mouse_r = 1;
	          break;
	      }
	      break;

	    case ButtonRelease:
	      switch (((XButtonEvent *)&event)->button)
	      {
	        case Button1:
	          pro_mouse_l = 0;
	          break;

	        case Button2:
	          pro_mouse_m = 0;
	          break;

	        case Button3:
	          pro_mouse_r = 0;
	          break;
	      }
	      break;

	    case EnterNotify:
	      if (pro_mouse_in == 0)
	      {
	        pro_screen_save_old_keyboard();
	        pro_mouse_in = 1;
	        pro_screen_restore_keyboard();
	        pro_screen_update_keys();
	      }
	      break;

	    case LeaveNotify:
	      if (pro_mouse_in == 1)
	      {
	        pro_screen_save_keys();
	        pro_mouse_in = 0;
	        pro_screen_restore_old_keyboard();
	      }
	      break;
	  }
	}

	/* Check if screen should be updated */

	if (expose)
	{
	  /* Clear the window */

	  XClearWindow(ProDisplay, ProWindow);

	  /* Clear display cache, forcing full update */

	  pro_clear_mvalid();
	}
}
#endif
