/* term_curses.c - Terminal screen update code; based on term_x11.c

   Copyright (c) 1997-2003, Sidik Isani (xhomer@isani.org)

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

 *
 * TODO:
 *   - Simh terminal settings are still trapping ^E.
 *   - Suspend curses control if at the simh> prompt (see man
 *     page for an example of how to do this.)
 *   - Check all keyboard mappings.
 *   - Assign alternate to CONTROL-F1 since not all text
 *     terminals may have such a key combination (linux console is
 *     one, for example, which cannot distinguish control-f1 and f1.)
 *   - Suppress error messages from xhomer itself which
 *     can corrupt the curses window.
 *   - Implement Control-L to force a refresh in case screen
 *     gets messed up anyway.
 *   - Warn if terminal window is smaller than 80x24.
 *   - Implement overlay so that underlying characters show even
 *     if the menu is up (right now, entire screen is replaced.)
 *   - Colors?
 *   - Search up/down by scanlines so that Synergy works.
 *   - ifdef-out some of the special KEY_ definitions which might
 *     be ncurses-specific (i.e., use them only if defined, otherwise
 *     this may fail to compile with older curses libraries.)
 */

#ifdef PRO

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curses.h>

#include "pro_defs.h"
#include "pro_lk201.h"

#define	PRO_KEYBOARD_FIFO_DEPTH	1024

#define PRO_FONT_SCANLINES 10

/*
 * Special key codes not defined by curses.
 */
#define ESCAPE (27)
#define CNTR(c) (1-'A'+c)
#define CNTR_F1 03776 /* Any unused keycode will do. */


/* Dummy values copied from term_x11.c - required so xhomer can compile. */
int                     pro_nine_workaround = 0;        /* workaround for #9 Xserver bugs */
int                     pro_libc_workaround = 0;        /* workaround for pre-glibc-2.0 bug */
int			pro_window_x = 0;	/* window x position */
int			pro_window_y = 0;	/* window y position */
int			pro_screen_full = 0;	/* 0 = window, 1 = full-screen */
int			pro_screen_window_scale = 2;	/* vertical window scale factor */
int			pro_screen_full_scale = 2;	/* vertical DGA scale factor */
int                     pro_screen_gamma = 10;          /* 10x gamma correction factor */
int                     pro_screen_pcm = 1;             /* 0 = don't use PCM, 1 = use PCM (private color map) */
int                     pro_screen_framebuffers = 1;    /* number of DGA framebuffers (1..3)
							 */

int			pro_mouse_x = 0;	/* mouse x */
int			pro_mouse_y = 0;	/* mouse y */
int			pro_mouse_l = 0;	/* left mouse button */
int			pro_mouse_m = 0;	/* middle mouse button */
int			pro_mouse_r = 0;	/* right mouse button */
int			pro_mouse_in = 0;	/* mouse in window */

LOCAL int		pro_screen_blank = 0;	/* indicates whether screen is blanked */

LOCAL WINDOW*           main_window = 0;
LOCAL WINDOW*           overlay_window = 0;

LOCAL int               titlebar_ok = 0;

/* Temporary hack for terminal types which have no "control-F1" (linux) */

LOCAL int               f1_equals_control_f1 = 0;

/* Put a title on the display window */

void pro_screen_title (char *title)
{
   if (titlebar_ok)
      fprintf(stderr, "\033]2;%s\a", title);
}

LOCAL void update ()
{
   wnoutrefresh(main_window);
   if (overlay_window)
   {
      touchwin(overlay_window);
      wnoutrefresh(overlay_window);
   }
   doupdate();
}

void pro_screen_clear ()
{
   wclear(main_window);
   update();
}

/* External keyboard polling routine */

LOCAL int keyboard_get ()
{
   static int shift = 0;
   static int ctrl = 0;
   static int ch = ERR;
   int shift_wanted;
   int ctrl_wanted;
   int c;

   while (ch==ERR)
   {
      static int escape = 0; /* >0 if reading an escape sequence */
      static char escape_seq[1024];

      wnoutrefresh(main_window);
      ch = wgetch(main_window);
      if (f1_equals_control_f1 && ch == KEY_F(1))
	 ch = CNTR_F1;
      if (escape)
      {
	 escape_seq[escape-1] = ch;
	 if (!(('0'<=ch && ch<=';') ||
               ch=='(' || ch=='[' || ch=='O' || ch==ESCAPE))
	    /* filter out escape sequences not handled by curses */
	 {
	    if (ch == ERR) escape--;
            escape_seq[escape]='\0';
            escape=0;
	    if (!strcmp(escape_seq, ""))
	       ch = ESCAPE;
	    else if (!strcmp(escape_seq, "[11^")) /* rxvt */
	       ch = CNTR_F1;
	    else if (!strcmp(escape_seq, "O5P")) /* xterm */
	       ch = CNTR_F1;
	    else if (!strcmp(escape_seq, "[[A")) /* linux, F1 */
	       ch = CNTR_F1;
            else if (!strcmp(escape_seq, "[5~"))
               ch = KEY_PPAGE;
            else if (!strcmp(escape_seq, "[6~"))
               ch = KEY_NPAGE;
            else if (!strcmp(escape_seq, "[3~"))
               ch = KEY_DC;
            else if (!strcmp(escape_seq, "Ot"))
               ch = KEY_LEFT;
            else if (!strcmp(escape_seq, "Ov"))
               ch = KEY_RIGHT;
            else if (!strcmp(escape_seq, "Ox"))
               ch = KEY_UP;
            else if (!strcmp(escape_seq, "Or"))
               ch = KEY_DOWN;
            else
            {
               beep();
	       /* DEBUG:
               fprintf(stderr, "Unrecognized escape sequence: `%.900s'", escape_seq);
	       */
               ch = ERR;
            }
	 }
         else
         {
            if (escape<1022) escape++;
         }
      }
      else if (ch == ESCAPE) escape = 1;

      if (ch == ERR) return PRO_NOCHAR;
      if (escape) { ch = ERR; continue; }
   }

   /*
    * Check proper state of the control key
    */
   if (ch < ' ' || ch == CNTR_F1)
      ctrl_wanted = 1; else ctrl_wanted = 0;

   if (ctrl_wanted && !ctrl)
   {
      ctrl = 1;
      return PRO_LK201_CTRL;
   }

   /*
    * Check proper state of the shift key
    */
   if (strchr("~!@#$%^&*()_+ABCDEFGHIJKLMNOPQRSTUVWXYZ{}|:\"<>?", ch))
      shift_wanted = 1; else shift_wanted = 0;

   if (shift_wanted && !shift)
   {
      shift = 1;
      return PRO_LK201_SHIFT;
   }

   if (shift != shift_wanted || ctrl != ctrl_wanted)
   {
      ctrl = 0; shift = 0;
      return PRO_LK201_ALLUPS;
   }

   c = ch;
   ch = ERR;
   
   /* DEBUG:
   fprintf(stderr, "\r\nctrl=%d shift=%d key=%d\r\n", ctrl, shift, c);
   */

   switch (c)
   {
      case CNTR('I'): case KEY_BTAB:	  return PRO_LK201_TAB;
      case CNTR('M'):			  return PRO_LK201_RETURN;
      case ' ':				  return PRO_LK201_SPACE;
      case 'a': case 'A': case CNTR('A'): return PRO_LK201_A;
      case 'b': case 'B': case CNTR('B'): return PRO_LK201_B;
      case 'c': case 'C': case CNTR('C'): return PRO_LK201_C;
      case 'd': case 'D': case CNTR('D'): return PRO_LK201_D;
      case 'e': case 'E': case CNTR('E'): return PRO_LK201_E;
      case 'f': case 'F': case CNTR('F'): return PRO_LK201_F;
      case 'g': case 'G': case CNTR('G'): return PRO_LK201_G;
      case 'h': case 'H': case CNTR('H'): return PRO_LK201_H;
      case 'i': case 'I': /* CNTR('I') */ return PRO_LK201_I;
      case 'j': case 'J': case CNTR('J'): return PRO_LK201_J;
      case 'k': case 'K': case CNTR('K'): return PRO_LK201_K;
      case 'l': case 'L': case CNTR('L'): return PRO_LK201_L;
      case 'm': case 'M': /* CNTR('M') */ return PRO_LK201_M;
      case 'n': case 'N': case CNTR('N'): return PRO_LK201_N;
      case 'o': case 'O': case CNTR('O'): return PRO_LK201_O;
      case 'p': case 'P': case CNTR('P'): return PRO_LK201_P;
      case 'q': case 'Q': case CNTR('Q'): return PRO_LK201_Q;
      case 'r': case 'R': case CNTR('R'): return PRO_LK201_R;
      case 's': case 'S': case CNTR('S'): return PRO_LK201_S;
      case 't': case 'T': case CNTR('T'): return PRO_LK201_T;
      case 'u': case 'U': case CNTR('U'): return PRO_LK201_U;
      case 'v': case 'V': case CNTR('V'): return PRO_LK201_V;
      case 'w': case 'W': case CNTR('W'): return PRO_LK201_W;
      case 'x': case 'X': case CNTR('X'): return PRO_LK201_X;
      case 'y': case 'Y': case CNTR('Y'): return PRO_LK201_Y;
      case 'z': case 'Z': case CNTR('Z'): return PRO_LK201_Z;
      case '`': case '~':                 return PRO_LK201_TICK;
      case '1': case '!':                 return PRO_LK201_1;
      case '2': case '@':                 return PRO_LK201_2;
      case '3': case '#':                 return PRO_LK201_3;
      case '4': case '$':                 return PRO_LK201_4;
      case '5': case '%':                 return PRO_LK201_5;
      case '6': case '^': case CNTR('^'): return PRO_LK201_6;
      case '7': case '&':                 return PRO_LK201_7;
      case '8': case '*':                 return PRO_LK201_8;
      case '9': case '(':                 return PRO_LK201_9;
      case '0': case ')':                 return PRO_LK201_0;
      case '-': case '_': case CNTR('_'): return PRO_LK201_MINUS;
      case '=': case '+':                 return PRO_LK201_EQUAL;
      case '[': case '{': /* CNTR('[') */ return PRO_LK201_LEFTB;
      case ']': case '}': case CNTR(']'): return PRO_LK201_RIGHTB;
      case '\\':case '|': case CNTR('\\'):return PRO_LK201_BACKSL;
      case ';': case ':':                 return PRO_LK201_SEMI;
      case '\'':case '"':                 return PRO_LK201_QUOTE;
      case ',': case '<':                 return PRO_LK201_COMMA;
      case '.': case '>':                 return PRO_LK201_PERIOD;
      case '/': case '?':                 return PRO_LK201_SLASH;
      case KEY_BREAK:			  return PRO_LK201_DO; /* %%% */
      case KEY_DOWN:			  return PRO_LK201_DOWN;
      case KEY_UP:			  return PRO_LK201_UP;
      case KEY_LEFT:	case KEY_SLEFT:   return PRO_LK201_LEFT;
      case KEY_RIGHT:	case KEY_SRIGHT:  return PRO_LK201_RIGHT;
      case KEY_HOME:	case KEY_SHOME:   return PRO_LK201_SELECT; /* %%% */
      case KEY_BACKSPACE:		return PRO_LK201_DEL;
      case ESCAPE:			return PRO_LK201_HOLD;
      case CNTR_F1:
      case KEY_F(1):			return PRO_LK201_PRINT;
      case KEY_F(2):			return PRO_LK201_SETUP;
      case KEY_F(3):			return PRO_LK201_FFOUR;
      case KEY_F(4):			return PRO_LK201_BREAK;
      case KEY_F(5):			return PRO_LK201_INT;
      case KEY_F(6):			return PRO_LK201_RESUME;
      case KEY_F(7):			return PRO_LK201_CANCEL;
      case KEY_F(8):			return PRO_LK201_MAIN;
      case KEY_F(9):			return PRO_LK201_EXIT;
      case KEY_F(10):			return PRO_LK201_ESC;
      case KEY_F(11):			return PRO_LK201_BS;
      case KEY_F(12):			return PRO_LK201_LF;
      /* %%% ADDOP, HELP, and DO! */
      case KEY_DL:	case KEY_SDL:
      case KEY_IL:
      case KEY_DC:	case KEY_SDC:
      case KEY_IC:	case KEY_SIC:
      case KEY_EIC:
      case KEY_CLEAR:
      case KEY_EOS:
      case KEY_EOL:	case KEY_SEOL:
      case KEY_SF:
      case KEY_SR:
      case KEY_NPAGE:			return PRO_LK201_NEXT;
      case KEY_PPAGE:			return PRO_LK201_PREV;
      case KEY_STAB:
      case KEY_CTAB:
      case KEY_CATAB:
      case KEY_ENTER:
      case KEY_SRESET:
      case KEY_RESET:
      case KEY_PRINT:
      case KEY_LL:
      case KEY_A1:
      case KEY_A3:
      case KEY_B2:
      case KEY_C1:
      case KEY_C3:
      case KEY_BEG:	case KEY_SBEG:
      case KEY_CANCEL:	case KEY_SCANCEL:
      case KEY_CLOSE:
      case KEY_COMMAND:	case KEY_SCOMMAND:
      case KEY_COPY:	case KEY_SCOPY:
      case KEY_CREATE:	case KEY_SCREATE:
      case KEY_END:
      case KEY_EXIT:	case KEY_SEXIT:
      case KEY_FIND:	case KEY_SFIND:
      case KEY_HELP:	case KEY_SHELP:
      case KEY_MARK:
      case KEY_MESSAGE:	case KEY_SMESSAGE:
      case KEY_MOVE:	case KEY_SMOVE:
      case KEY_NEXT:	case KEY_SNEXT:
      case KEY_OPEN:
      case KEY_OPTIONS:	case KEY_SOPTIONS:
      case KEY_PREVIOUS: case KEY_SPREVIOUS:
      case KEY_REDO:	case KEY_SREDO:
      case KEY_REFERENCE:
      case KEY_REFRESH:
      case KEY_REPLACE:	case KEY_SREPLACE:
      case KEY_RESTART:
      case KEY_RESUME:	case KEY_SRSUME:
      case KEY_SAVE:	case KEY_SSAVE:
      case KEY_SELECT:
      case KEY_SEND:
      case KEY_SUSPEND:	case KEY_SSUSPEND:
      case KEY_UNDO:	case KEY_SUNDO:
      default:
	return PRO_NOCHAR;
   }
}

int pro_keyboard_get ()
{
   return pro_menu(keyboard_get());
}

/* Initialize the display */

LOCAL int pro_screen_init_curses ()
{
   static int initialized = 0;

   if (!initialized)
   {
      initialized = 1;

      initscr(); cbreak(); noecho(); nonl(); start_color();
      if ((main_window = newwin(0, 0, 0, 0))==0)
	 return PRO_FAIL;
   }
   else
   {
      /* === TODO: Window resize stuff goes here? */
      /* Window resize really shouldn't be allowed (texthomer requires
       * 80x24 to work properly.)
       */
   }
   leaveok(main_window, TRUE); /* Ok to leave cursor when redrawing screen */
   nodelay(main_window, TRUE); /* wgetch() returns ERR if no chars */
   intrflush(main_window, TRUE); /* flush input if ^C is hit */
   keypad(main_window, TRUE); /* enable numeric keypad codes */
   curs_set(1); /* Select small cursor */
   curs_set(0); /* Or better still, no cursor */
   return PRO_SUCCESS;
}

int pro_screen_init ()
{
   const char* term;
   static char termvar[256];

   if (!(term=getenv("COLORTERM")) || !*term)
      if (!(term=getenv("TERM")) || !*term)
	 term = "vt100";

   sprintf(termvar, "TERM=%.240s", term);
   putenv(termvar);

   if (!strncmp(term, "xterm", 5) ||
       !strncmp(term, "rxvt", 4))
      titlebar_ok = 1;
   else
      titlebar_ok = 0;

   if (getenv("TERM") && !strncmp(getenv("TERM"), "linux", 5))
      f1_equals_control_f1 = 1;

   if (pro_screen_init_curses()==PRO_FAIL)
   {
      fprintf(stderr, "pro_curses: trouble initializing ncurses.\n");
      return PRO_FAIL;
   }

   /* Clear image buffer */
   
   pro_screen_clear();
   
   /* Invalidate display cache */
   
   pro_clear_mvalid();

   return PRO_SUCCESS;
}


void pro_screen_close ()
{
   endwin();
}

/* Reset routine (called only once) */

void pro_screen_reset ()
{
   /* %%% */
}


/* This function is called whenever the colormap mode changes.
   A new X11 colormap is loaded for private colormap modes,
   mvalid is cleared otherwise */

void pro_mapchange ()
{
   /* === TODO: Support colors with ncurses? */
}

void pro_colormap_write (int index, int rgb)
{
   /* === TODO: Support colors with ncurses? */
}


/* This is called whenever the scroll register changes */

void pro_scroll ()
{
   /* Clear entire display cache for now %%% */
   pro_clear_mvalid();
}


/* This called every emulated vertical retrace */

#include "term_fonthash_curses.c"

void pro_screen_update ()
{
   int x, y, i;
   int vindex = vmem(0);
   int need_update = 0;

   /* Check whether screen has been blanked */
   
   if ((pro_vid_p1c & PRO_VID_P1_HRS) == PRO_VID_P1_OFF)
   {
      if (pro_screen_blank == 0)
      {
	 /* Blank the screen */
	 /* %%% */
	 pro_screen_clear();
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
   }

   /* Redraw portions of screen that have changed */
#ifdef TEXT_PIXEL_ZOOM
   /* Experimental mode with 1-character per pixel */
   for (y=0; y<PRO_VID_SCRHEIGHT; y++)
   {
      int invalid = 0;

      if (pro_vid_mvalid[cmem(vindex)] == 0)
      {
	 pro_vid_mvalid[cmem(vindex)] = 1;
	 invalid = 1; /* If any row has a change, redo this line of text */
      }
      if (invalid)
      {
	 need_update = 1;
	 for(i=0; i<(PRO_VID_SCRWIDTH/16); i++)
	 {
	    int vpix;
	    int vpixs = i * 16;
	    int vpixe = vpixs + 16;
	    
	    int vdata0 = PRO_VRAM[0][vindex+i];
	    int vdata1 = PRO_VRAM[1][vindex+i] << 1;
	    int vdata2 = PRO_VRAM[2][vindex+i] << 2;
	    
	    for(vpix=vpixs; vpix<vpixe; vpix++)
	    {
	       int cindex = (vdata2 & 04) | (vdata1 & 02) | (vdata0 & 01);
	       int color = cindex; /* pro_lut[cindex]; === */
	       
	       mvwaddch(main_window, y, vpix-8-2*12, color>0?'*':' ');
	       
	       vdata0 = vdata0 >> 1;
	       vdata1 = vdata1 >> 1;
	       vdata2 = vdata2 >> 1;
	    }
	 }
      }	 
      vindex = (vindex + (PRO_VID_SCRWIDTH/16)) & PRO_VID_VADDR_MASK;
   }
#else
   for (y=0; y<PRO_VID_SCRHEIGHT/PRO_FONT_SCANLINES; y++)
   {
      int vlist[PRO_FONT_SCANLINES];
      int invalid = 0;

      for (i=0; i<10; i++) /* 10 pixel rows in each font cell */
      {
	 vlist[i] = vindex;
	 if (!pro_vid_mvalid[cmem(vindex)])
	 {
	    pro_vid_mvalid[cmem(vindex)] = 1;
	    invalid = 1; /* If any row has a change, redo this line of text */
	 }
	 vindex = (vindex + (PRO_VID_SCRWIDTH/16)) & PRO_VID_VADDR_MASK;
      }

      /* Update screen segment only if display cache is invalid */
      /* %%% Check that PRO_VID_SCRWIDTH == PRO_VID_CLS_PIX */

      if (invalid) for (x=0; x<80; x++)
      {
	 char fontcell[PRO_FONT_SCANLINES*2 + 1];
	 register unsigned short tmp;
	 int ofs;

	 need_update = 1;

	 /* [00000000 00001111] [11111111 22222222] [22223333 33333333] */
	 /* [11110000 00000000] [22222222 11111111] [33333333 33332222] */
#define OFS 2
	 fontcell[PRO_FONT_SCANLINES*2] = '\0';
	 switch (x % 4)
	 {
	    case 0:
	    {
	       for (i=0; i<PRO_FONT_SCANLINES; i++)
	       {
		  ofs = vlist[i] + OFS + (x/4)*3;
		  tmp = PRO_VRAM[0][ofs];
		  fontcell[i*2]   = '0' + ((tmp) & 0x3F);
		  fontcell[i*2+1] = '0' + ((tmp >> 6) & 0x3F);
	       }
	       break;
	    }
	    case 1:
	    {
	       for (i=0; i<PRO_FONT_SCANLINES; i++)
	       {
		  ofs = vlist[i] + OFS + (x/4)*3;
		  tmp = PRO_VRAM[0][ofs+1];
		  fontcell[i*2+1] = '0' + ((tmp >> 2) & 0x3F);
		  tmp <<= 4;
		  tmp |= (PRO_VRAM[0][ofs]>>12);
		  fontcell[i*2]   = '0' + ((tmp) & 0x3F);
	       }
	       break;
	    }
	    case 2:
	    {
	       for (i=0; i<PRO_FONT_SCANLINES; i++)
	       {
		  ofs = vlist[i] + OFS + (x/4)*3;
		  tmp = PRO_VRAM[0][ofs+1];
		  fontcell[i*2]   = '0' + ((tmp >> 8) & 0x3F);
		  tmp >>= 14;
		  tmp |= (PRO_VRAM[0][ofs+2]<<2);
		  fontcell[i*2+1] = '0' + ((tmp) & 0x3F);
	       }
	       break;
	    }
	    case 3:
	    {
	       for (i=0; i<PRO_FONT_SCANLINES; i++)
	       {
		  ofs = vlist[i] + OFS + (x/4)*3;
		  tmp = PRO_VRAM[0][ofs+2];
		  fontcell[i*2]   = '0' + ((tmp >> 4) & 0x3F);
		  fontcell[i*2+1] = '0' + ((tmp >> 10) & 0x3F);
	       }
	       break;
	    }
	 }
	 {
	    int hashed = hash(fontcell);
	    int bold = 0, reverse = 0;

	    if (hashed > 511) { hashed -= 512; reverse=1; }
	    if (hashed > 255) { hashed -= 256; bold=1; }
	    switch (hashed)
	    {
	       case 0: hashed = ' '; break;
	       case 30: hashed = ACS_BLOCK; break;
	       case 128: hashed = ACS_DIAMOND; break;
	       case 129: hashed = ACS_CKBOARD; break;
	       case 134: hashed = ACS_DEGREE; break;
	       case 135: hashed = ACS_PLMINUS; break;
	       case 138: hashed = ACS_LRCORNER; break;
	       case 139: hashed = ACS_URCORNER; break;
	       case 140: hashed = ACS_ULCORNER; break;
	       case 141: hashed = ACS_LLCORNER; break;
	       case 142: hashed = ACS_PLUS; break;
	       case 143: hashed = ACS_S1; break;
#ifdef ACS_S3
	       case 144: hashed = ACS_S3; break;
#endif
	       case 145: hashed = ACS_HLINE; break;
#ifdef ACS_S7
	       case 146: hashed = ACS_S7; break;
#endif
	       case 147: hashed = ACS_S9; break;
	       case 148: hashed = ACS_LTEE; break;
	       case 149: hashed = ACS_RTEE; break;
	       case 150: hashed = ACS_BTEE; break;
	       case 151: hashed = ACS_TTEE; break;
	       case 152: hashed = ACS_VLINE; break;
#ifdef ACS_LEQUAL
	       case 153: hashed = ACS_LEQUAL; break;
#endif
#ifdef ACS_GEQUAL
	       case 154: hashed = ACS_GEQUAL; break;
#endif
#ifdef ACS_PI
	       case 155: hashed = ACS_PI; break;
#endif
#ifdef ACS_NEQUAL
	       case 156: hashed = ACS_NEQUAL; break;
#endif
#ifdef ACS_STERLING
	       case 157: hashed = ACS_STERLING; break;
#endif
	       case 158: hashed = ACS_BULLET; break;
	       default: if (hashed < ' ' || hashed > '~') hashed = '#';
	    }
	    mvwaddch(main_window, y, x, hashed|(bold?A_BOLD:0)|(reverse?A_REVERSE:0));
	 }
      }
   }
#endif

   /* Flush the changes to the screen */

   if (need_update) update();
}


/* Set keyboard bell volume */

void pro_keyboard_bell_vol (int vol)
{
   /* Not implemented for curses */
}


/* Sound keyboard bell */

void pro_keyboard_bell ()
{
   fputc('\a', stderr);
}


/* Turn off auto-repeat */

void pro_keyboard_auto_off ()
{
   /* Not implemented for curses */
}


/* Turn on auto-repeat */

void pro_keyboard_auto_on ()
{
   /* Not implemented for curses */
}


/* Turn off keyclick */

void pro_keyboard_click_off ()
{
   /* Not implemented for curses */
}


/* Turn on keyclick */

void pro_keyboard_click_on ()
{
   /* Not implemented for curses */
}

void pro_overlay_print(int x, int y, int xnor, int font, char *text)
{
   if (!overlay_window) return;
   if (font) wattron(overlay_window, A_BOLD);
   wmove(overlay_window, y, x);
   for (; *text; text++)
   {
      if (*text == ' ' && xnor==1)
      {
	 chtype ch = winch(overlay_window);
	 waddch(overlay_window, ch^A_REVERSE);
      }
      else if (' ' <= *text && *text <= '~')
      {
	 waddch(overlay_window, *text);
      }
      else switch (*text)
      {
	 case 24: waddch(overlay_window, ACS_ULCORNER); break;
	 case 25:
	 case 29: waddch(overlay_window, ACS_HLINE); break;
	 case 26: waddch(overlay_window, ACS_URCORNER); break;
	 case 27: 
	 case 31: waddch(overlay_window, ACS_VLINE); break;
	 case 30: waddch(overlay_window, ACS_LLCORNER); break;
	 case 28: waddch(overlay_window, ACS_LRCORNER); break;
	 default: waddch(overlay_window, ACS_BLOCK);
      }
   }
   if (font) wattroff(overlay_window, A_BOLD);
   update();
   return;
}

void pro_overlay_disable ()
{
   if (!overlay_window) return;
   delwin(overlay_window);
   overlay_window = 0;
   touchwin(main_window);
   update();
}

void pro_overlay_enable ()
{
   pro_overlay_disable();
   overlay_window = newwin(0, 0, 0, 0);
}

#endif
