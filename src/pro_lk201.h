/* pro_lk201.h: lk201 keycodes and other definitions

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


#define PRO_LK201_HOLD		86
#define PRO_LK201_PRINT		87
#define PRO_LK201_SETUP		89
#define PRO_LK201_FFOUR		90 /*XXX correct? */
#define PRO_LK201_BREAK		88
#define PRO_LK201_INT		100
#define PRO_LK201_RESUME	101
#define PRO_LK201_CANCEL	102
#define PRO_LK201_MAIN		103
#define PRO_LK201_EXIT		104
#define PRO_LK201_ESC		113
#define PRO_LK201_BS		114
#define PRO_LK201_LF		115
#define PRO_LK201_ADDOP		116
#define PRO_LK201_HELP		124
#define PRO_LK201_DO		125
#define PRO_LK201_F17		128
#define PRO_LK201_F18		129
#define PRO_LK201_F19		130
#define PRO_LK201_F20		131

#define PRO_LK201_PF1		161
#define PRO_LK201_PF2		162
#define PRO_LK201_PF3		163
#define PRO_LK201_PF4		164

#define PRO_LK201_A		194
#define PRO_LK201_B		217
#define PRO_LK201_C		206
#define PRO_LK201_D		205
#define PRO_LK201_E		204
#define PRO_LK201_F		210
#define PRO_LK201_G		216
#define PRO_LK201_H		221
#define PRO_LK201_I		230
#define PRO_LK201_J		226
#define PRO_LK201_K		231
#define PRO_LK201_L		236
#define PRO_LK201_M		227
#define PRO_LK201_N		222
#define PRO_LK201_O		235
#define PRO_LK201_P		240
#define PRO_LK201_Q		193
#define PRO_LK201_R		209
#define PRO_LK201_S		199
#define PRO_LK201_T		215
#define PRO_LK201_U		225
#define PRO_LK201_V		211
#define PRO_LK201_W		198
#define PRO_LK201_X		200
#define PRO_LK201_Y		220
#define PRO_LK201_Z		195

#define PRO_LK201_0		239
#define PRO_LK201_1		192
#define PRO_LK201_2		197
#define PRO_LK201_3		203
#define PRO_LK201_4		208
#define PRO_LK201_5		214
#define PRO_LK201_6		219
#define PRO_LK201_7		224
#define PRO_LK201_8		229
#define PRO_LK201_9		234

#define PRO_LK201_LT		201
#define PRO_LK201_MINUS		249
#define PRO_LK201_EQUAL		245
#define PRO_LK201_LEFTB		250
#define PRO_LK201_RIGHTB	246

#define PRO_LK201_SEMI		242
#define PRO_LK201_QUOTE		251
#define PRO_LK201_BACKSL	247
#define PRO_LK201_COMMA		232
#define PRO_LK201_PERIOD	237
#define PRO_LK201_SLASH		243
#define PRO_LK201_TICK		191

#define PRO_LK201_SPACE		212

#define PRO_LK201_DEL		188
#define PRO_LK201_RETURN	189
#define PRO_LK201_TAB		190

#define PRO_LK201_UP		170
#define PRO_LK201_DOWN		169
#define PRO_LK201_LEFT		167
#define PRO_LK201_RIGHT		168

#define PRO_LK201_FIND		138
#define PRO_LK201_INSERT	139
#define PRO_LK201_REMOVE	140
#define PRO_LK201_SELECT	141
#define PRO_LK201_PREV		142
#define PRO_LK201_NEXT		143

#define PRO_LK201_LOCK		176
#define PRO_LK201_SHIFT		174
#define PRO_LK201_CTRL		175
#define PRO_LK201_COMPOSE	177

#define PRO_LK201_ALLUPS	179
#define PRO_LK201_LCK_ACK	183

#define PRO_LK201_REPEAT	180

/* Commands */

#define PRO_LK201_UNLCK		139	/* resume keyboard transmission */
#define PRO_LK201_LCK		137	/* inhibit keyboard transmission */
#define PRO_LK201_BELL_D	161	/* disable bell */
#define PRO_LK201_BELL_E	35	/* enable bell */
#define PRO_LK201_BELL		167	/* sound bell */
#define PRO_LK201_ID		171	/* request keyboard ID */
#define PRO_LK201_PWRUP		253	/* jump to power up */
#define PRO_LK201_TEST		203	/* jump to test mode */
#define PRO_LK201_DEF		211	/* reinstate defaults */
#define PRO_LK201_AUTO_D	225	/* disable auto-repeat */
#define PRO_LK201_AUTO_E	227	/* enable auto-repeat */
#define PRO_LK201_CLICK_D	153	/* disable keyclick */
#define PRO_LK201_CLICK_E	27	/* enable keyclick */
#define PRO_LK201_DECTOUCH	254	/* XXX undocumented DECtouch query command */
#define PRO_LK201_NOCMD		-1	/* no command */

/* DECtouch response codes */

#define PRO_LK201_DT		0x3c	/* DECtouch present, no keyboard */
#define PRO_LK201_DTKB		0x3d	/* DECtouch and keyboard present */
#define PRO_LK201_KB		0xb6	/* keyboard present, no DECtouch */

/* IDs */

#define PRO_LK201_IDF		1	/* firmware ID */
#define PRO_LK201_IDH		0	/* hardware ID */
#define PRO_LK201_NOERR		0	/* no error */

/* Misc */

#define PRO_LK201_PARAMS	0x80	/* parameter following flag */
#define PRO_LK201_VOL		0x07	/* volume parameter */
