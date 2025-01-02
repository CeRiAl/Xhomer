/* pro_defs.h: various global defnitions

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


#include "pro_version.h"
#include "pro_config.h"

/* This macro handles byte and word write access to "reg" with
   write-enable mask "we" */

#define WRITE_WB(reg, we, access) \
  if ((access) == WRITEB) (reg) = (pa & 1)? \
    (((reg) & ~(we)) \
    | ((data << 8) & (we)) | ((reg) & 0377)): \
    (((reg) & ~(we)) \
    | ((data & 0377) & (we)) | ((reg) & ~0377)); \
  else \
    reg = ((reg) & ~(we)) \
          | (data & (we))


/* This one handles only word accesses */

#define WRITE_W(reg, we) \
  reg = ((reg) & ~(we)) \
        | (data & (we))


/* This macro sets a single bit in a register */

#define pro_setbit(reg, bit) \
  reg = reg | (1 << bit)


/* This macro clears a single bit in a register */

#define pro_clrbit(reg, bit) \
  reg = reg & ~(1 << bit)


/* Uses scroll register offset to calculate video buffer address */

#define vmem(vaddr) \
  (((vaddr) + pro_vid_y_offset) & PRO_VID_VADDR_MASK)

/* Uses frame buffer address to calculate cache tag address */

#define cmem(index) \
  ((index) >> PRO_VID_CLS)


/* Serial line control structure */

struct serctrl
	{
	  int	stop;
	  int	parity;
	  int	penable;
	  int	cs;
	  int	ibaud;
	  int	obaud;
	  int	dtr;
	  int	dsr;
	  int	rts;
	  int	cts;
	  int	ri;
	  int	cd;
	  int	ibrk;
	  int	obrk;
	  int	perror;
	};

/* Entry points into serial I/O routines */

struct sercall
	{
	  int	(*get)();
	  int	(*put)();
	  void	(*ctrl_get)();
	  void	(*ctrl_put)();
	  void	(*reset)();
	  void	(*exit)();
	};


/* System configuration */

#if (PRO_BASE_1M == 0)
#define PRO_CSR_INIT		0025		/* 256K */
#else
#define PRO_CSR_INIT		0037		/* 1M */
#endif

#if (PRO_EBO_PRESENT == 1)
#define PRO_VID_CSR		0000000
#define PRO_VID_NUMPLANES	3
#else
#define PRO_VID_CSR		0020000
#define PRO_VID_NUMPLANES	1
#endif

/* CTI Bus Board IDs */

#define PRO_ID_RD		0000401
#define PRO_ID_RX		0000004
#define PRO_ID_VID		0177402
#define PRO_ID_EBO		0177403
#define PRO_ID_MEM		0000034

/* Option module present register */

#define PRO_OMP_RD		0001	/* RD50 in slot 1 */
#define PRO_OMP_RX		0002	/* RX50 in slot 2 */
#define PRO_OMP_VID		0004	/* Video in slot 3 */
#define PRO_OMP_EBO		0010	/* Extended bitmap in slot 4 */
#define PRO_OMP_MEM		0020	/* Memory in slot 5 */

#define PRO_OMP		(PRO_RD_PRESENT*PRO_OMP_RD \
			| PRO_RX_PRESENT*PRO_OMP_RX \
			| PRO_VID_PRESENT*PRO_OMP_VID \
			| PRO_EBO_PRESENT*PRO_OMP_EBO \
			| PRO_MEM_PRESENT*PRO_OMP_MEM) 


/* Register writeable bit definitions */

/* System module */

#define PRO_LED_W		0000017
#define PRO_CSR_W		0000200
#define PRO_BATRAM_W		0000377

/* RX50 controller */

#define PRO_RX_CSR0_C_W		0000377
#define PRO_RX_CSR1_C_W		0000377
#define PRO_RX_CSR2_C_W		0000377
#define PRO_RX_CSR3_C_W		0000377
#define PRO_RX_CSR5_C_W		0000377

/* RD controller */

#define PRO_RD_EP_W		0000377
#define PRO_RD_SEC_W		0177437
#define PRO_RD_CYL_W		0001777
#define PRO_RD_HEAD_W		0000007
#define PRO_RD_S2C_W		0000377

/* Video board */

#define PRO_VID_CSR_W		0043503
#define PRO_VID_P1C_W		0000077
#define PRO_VID_OPC_W		0037477
#define PRO_VID_CMP_W		0177777
#define PRO_VID_SCL_W		0000377
#define PRO_VID_X_W		0001777
#define PRO_VID_Y_W		0000377
#define PRO_VID_CNT_W		0177777
#define PRO_VID_PAT_W		0177777
#define PRO_VID_MBR_W		0000177

/* Realtime clock */

#define PRO_CLK_CSR0_W		0000177
#define PRO_CLK_CSR1_W		0000377
#define PRO_CLK_AL_SEC_W	0000377
#define PRO_CLK_AL_MIN_W	0000377
#define PRO_CLK_AL_HOUR_W	0000377

/* Keyboard and printer controllers */

#define PRO_2661_DATA_W		0000377
#define PRO_2661_MR1_W		0000377
#define PRO_2661_MR2_W		0000377
#define PRO_2661_CMD_W		0000357

/* Communication port controller */

#define PRO_COM_W		0000377

/* Memory expansion board */

#define PRO_MEM_CMD_W		0000207

/* Clock constants */

#define PRO_CLK_AL_X		0300		/* alarm "don't care" bits */
#define PRO_CLK_UIP		0200		/* update in progress */
#define PRO_CLK_SET		0200		/* set realtime clock */
#define PRO_CLK_PIE		0100		/* periodic int enable */
#define PRO_CLK_AIE		0040		/* alarm int enable */
#define PRO_CLK_UIE		0020		/* update-ended int enable */
#define PRO_CLK_IRQF		0200		/* interrupt request flag */
#define PRO_CLK_PF		0100		/* periodic interrupt flag */
#define PRO_CLK_AF		0040		/* alarm flag */
#define PRO_CLK_UF		0020		/* update ended flag */
#define PRO_CLK_VRT		0200		/* valid RAM and time */
#define PRO_CLK_RS		0017		/* rate select bits */
#define PRO_CLK_DM		0004		/* data mode */
#define PRO_CLK_24H		0002		/* 24 hour mode */
#define PRO_CLK_RTC_SWITCH	6000000		/* number of instructions simulated
						   before switching to realtime mode */
#define PRO_CLK_RTC_CATCHUP	1000		/* time until RTC catchup interrupt */


/* Keyboard and printer controller constants */

#define PRO_2661_RD		0002		/* receiver done */
#define PRO_2661_TR		0001		/* transmitter ready */
#define PRO_2661_OM		0300		/* operating mode */
#define PRO_2661_OM1		0200		/* operating mode */
#define PRO_2661_OM0		0100		/* operating mode */
#define PRO_2661_RXEN		0004		/* receiver enable */
#define PRO_2661_TXEN		0001		/* transmitter enable */
#define PRO_2661_LOOPBACK	0200		/* local loopback */
#define PRO_2661_NORMAL		0000		/* normal operation */
#define PRO_2661_DSR		0200		/* DSR input pin */
#define PRO_2661_DTR		0002		/* DTR output pin */
#define PRO_2661_FB		0010		/* force break */
#define PRO_2661_CL		0014		/* character length */
#define PRO_2661_SBL		0300		/* stop bit length */
#define PRO_2661_PT		0040		/* parity type */
#define PRO_2661_PC		0020		/* parity control */
#define PRO_2661_BAUD		0017		/* baud */

/* Maintenance port constants */

#define PRO_MAINT_RD		0200		/* receiver done */
#define PRO_MAINT_TR		0200		/* transmitter ready */

/* Communication port constants */

#define PRO_COM_RP		0007	/* register pointer */
#define PRO_COM_CMD		0070
#define PRO_COM_EOI		0070	/* end of interrupt */
#define PRO_COM_VEC		0343	/* interrupt vector mask */
#define PRO_COM_IR_RCV		0030	/* receiver interrupt */
#define PRO_COM_IR_XMIT		0020	/* transmitter interrupt (XXX manual says 0000) */
#define PRO_COM_IR_NONE		0034	/* no interrupt pending */
#define PRO_COM_RIE		0030	/* receiver interrupt enable */
#define PRO_COM_TIE		0002	/* transmitter interrupt enable */
#define PRO_COM_RCL1		0200	/* receiver character length 1 */
#define PRO_COM_RCL0		0100	/* receiver character length 0 */
#define PRO_COM_RXEN		0001	/* receiver enable */
#define PRO_COM_SB		0014	/* stop bits */
#define PRO_COM_EO		0002	/* even/odd parity */
#define PRO_COM_PEN		0001	/* parity enable */
#define PRO_COM_SBRK		0020	/* send break */
#define PRO_COM_TXEN		0010	/* transmitter enable */
#define PRO_COM_MM		0200	/* maintenance mode */
#define PRO_COM_DTR		0020	/* dtr */
#define PRO_COM_RTS		0010	/* rts */
#define PRO_COM_TBR		0360	/* transmitter baud rate */
#define PRO_COM_RBR		0017	/* receiver baud rate */
#define PRO_COM_TU		0100	/* transmitter underrun */
#define PRO_COM_TBMT		0004	/* transmit buffer empty */
#define PRO_COM_INTP		0002	/* interrupt pending */
#define PRO_COM_RXCA		0001	/* receive character available */
#define PRO_COM_FE		0100	/* receiver framing error */
#define PRO_COM_RXPE		0020	/* receiver parity error */
#define PRO_COM_AS		0001	/* all sent */
#define PRO_COM_DSR		0200	/* dsr */
#define PRO_COM_RI		0100	/* ri */
#define PRO_COM_CTS		0040	/* cts */
#define PRO_COM_CD		0020	/* cd */
#define PRO_COM_INTQ_DEPTH	16	/* com pending interrupt queue depth */
#define PRO_COM_DATAQ_DEPTH	6	/* com read data queue depth */


/* Interrupt controller preselect codes */

#define PRO_PRESEL_IMR		0
#define PRO_PRESEL_ACR		1
#define PRO_PRESEL_RESP		2

#define PRO_NO_INT		255

#define PRO_INT_MM		0200	/* master mask bit in mode reg */
#define PRO_INT_IM		0004	/* interrupt mode bit in mode reg */
#define PRO_INT_VS		0002	/* vector selection bit in mode reg */
#define PRO_INT_PM		0001	/* priority mode bit in mode reg */

/* Interrupt codes */
/* MSB is controller number, LSB is interrupt bit */

#define	PRO_INT_KB_RCV		0x0002	/* keyboard receiver */
#define	PRO_INT_KB_XMIT		0x0004	/* keyboard transmitter */
#define	PRO_INT_COM		0x0008	/* communications port */
#define	PRO_INT_MODEM		0x0010	/* modem controls change */
#define	PRO_INT_PTR_RCV		0x0020	/* printer receiver */
#define	PRO_INT_PTR_XMIT	0x0040	/* printer transmitter */
#define	PRO_INT_CLK		0x0080	/* clock */

#define	PRO_INT_0A		0x0101	/* option module 0, int A */
#define	PRO_INT_1A		0x0102	/* option module 1, int A */
#define	PRO_INT_2A		0x0104	/* option module 2, int A */
#define	PRO_INT_3A		0x0108	/* option module 3, int A */
#define	PRO_INT_4A		0x0110	/* option module 4, int A */
#define	PRO_INT_5A		0x0120	/* option module 5, int A */

#define	PRO_INT_0B		0x0201	/* option module 0, int B */
#define	PRO_INT_1B		0x0202	/* option module 1, int B */
#define	PRO_INT_2B		0x0204	/* option module 2, int B */
#define	PRO_INT_3B		0x0208	/* option module 3, int B */
#define	PRO_INT_4B		0x0210	/* option module 4, int B */
#define	PRO_INT_5B		0x0220	/* option module 5, int B */

/* Event Queue codes */

#define PRO_EQ_BITS		19	/* event queue size (2^19 = 512K) */
#define PRO_EQ_SIZE		(1<<PRO_EQ_BITS)
#define PRO_EQ_MASK		(PRO_EQ_SIZE - 1)

#define PRO_EQ_CLK_IPS		(40*8192)	/* SOBs/sec */
#define PRO_EQ_CLK_UF		((PRO_EQ_CLK_IPS*300)/1000000)
				/* update-ended event - 300 uS */

/* XXX make these more accurate */

#define PRO_EQ_PAL		9000	/* instructions/PAL (50 Hz) field */
#define PRO_EQ_NTSC		7500	/* instructions/NTSC (60 Hz) field */
#define PRO_EQ_CMD		1	/* time to complete video command */
#define PRO_EQ_KB		2	/* loopback test delay */
#define PRO_EQ_KB_POLL		10000	/* keyboard polling interval */
#define PRO_EQ_KB_RETRY		1000	/* write retry delay */
#define PRO_EQ_PTR		2	/* loopback test delay */
#define PRO_EQ_PTR_POLL		100	/* printer polling interval */
#define PRO_EQ_PTR_RETRY	1000	/* write retry delay */
#define PRO_EQ_COM		3	/* loopback test delay */
#define PRO_EQ_COM_POLL		100	/* communication port polling interval */
#define PRO_EQ_COM_RETRY	1000	/* write retry delay */
#define PRO_EQ_RD		5	/* hard disk read/write delay (at least 5 for diagnostics) */


#define PRO_EQ_NUM_EVENTS	11

#define PRO_EVENT_CLK		1
#define PRO_EVENT_CLK_UF	2
#define PRO_EVENT_VID		3
#define PRO_EVENT_VID_CMD	4
#define PRO_EVENT_KB		5
#define PRO_EVENT_KB_POLL	6
#define PRO_EVENT_PTR		7
#define PRO_EVENT_PTR_POLL	8
#define PRO_EVENT_COM		9
#define PRO_EVENT_COM_POLL	10
#define PRO_EVENT_RD		11

/* Video codes */

#define PRO_VID_P1_VME		0000040		/* plane 1 video memory enable */
#define PRO_VID_P2_VME		0000040		/* plane 2 video memory enable */
#define PRO_VID_P3_VME		0020000		/* plane 3 video memory enable */

#define PRO_VID_VADDR_MASK	0037777		/* mask for 14 address */
#define PRO_VID_MEMSIZE		32*1024		/* size of one plane buffer */
#define PRO_VID_MEMWIDTH	1024		/* width of entire frame buffer */
#define PRO_VID_MEMHEIGHT	256		/* height of entire frame buffer */
#define PRO_VID_SCRWIDTH	1024		/* screen width */
#define PRO_VID_SCRHEIGHT	240		/* screen height */

#define PRO_VID_SKIP		0		/* number of window update cycles to skip */

#define PRO_VID_CLS		6		/* frame buffer cache line size (0-6) */
#define PRO_VID_CLS_PIX		(16 << PRO_VID_CLS)		/* cache line size in pixels */

#define PRO_VID_P1_HRS		0000030		/* plane 1 horizontal resolution select */
#define PRO_VID_P1_1024		0000000		/* plane 1 1024 pixel resolution */
#define PRO_VID_P1_OFF		0000030		/* plane 1 display off */

#define PRO_VID_DIE		0040000		/* done interrupt enable */
#define PRO_VID_OC1		0001000		/* operation class bit 1, plane 1 */
#define PRO_VID_OC0		0000400		/* operation class bit 0, plane 1 */
#define PRO_VID_EFIE		0000100		/* end of frame interrupt enable */
#define PRO_VID_LM		0000001		/* line mode - NTSC (0), PAL (1) */
#define PRO_VID_TD		0100000		/* transfer done */
#define PRO_VID_P1_OP		0000007		/* plane 1 logical operation select */
#define PRO_VID_P2_OP		0000007		/* plane 2 logical operation select */
#define PRO_VID_P3_OP		0003400		/* plane 3 logical operation select */
#define PRO_VID_CME		0002000		/* color map enable */
#define PRO_VID_R_MASK		0000340		/* red mask */
#define PRO_VID_G_MASK		0000034		/* green mask */
#define PRO_VID_B_MASK		0000003		/* blue mask */
#define PRO_VID_INDEX		0003400		/* colormap index */
#define PRO_VID_RGB		0000377		/* colormap rgb mask */

/* Pixel operations */

#define PRO_VID_PIX_NOP		000		/* nop */
#define PRO_VID_PIX_XOR		001		/* xor */
#define PRO_VID_PIX_SET		002		/* set pixel */
#define PRO_VID_PIX_CLR		003		/* clear pixel */
#define PRO_VID_PIX_REP		004		/* replace pixel */

/* Bit mode logical operations */

#define PRO_VID_BM_NOP		00		/* nop */
#define PRO_VID_BM_XOR		01		/* xor */
#define PRO_VID_BM_MOVE		02		/* move pattern to screen  */
#define PRO_VID_BM_NMOVE	03		/* move pattern complement to screen  */
#define PRO_VID_BM_SETPAT	04		/* bit set pattern to screen  */
#define PRO_VID_BM_CLRPAT	05		/* bit clear pattern to screen  */
#define PRO_VID_BM_CLRBIT	06		/* bit clear */
#define PRO_VID_BM_SETBIT	07		/* bit set */

/* Word mode logical operations */

#define PRO_VID_WM_NOP		00		/* nop */
#define PRO_VID_WM_COM		01		/* complement screen */
#define PRO_VID_WM_MOVE		02		/* move pattern to screen */
#define PRO_VID_WM_NMOVE	03		/* move pattern complement to screen */
#define PRO_VID_WM_S1		05		/* shift screen 1 bit */
#define PRO_VID_WM_S2		06		/* shift screen 2 bits */
#define PRO_VID_WM_S4		07		/* shift screen 4 bits */

/* RX50 controller */

/* Version 11 is the first that has (undocumented) format capability */

#define PRO_RX_VERSION		0001		/* version of RX50 controller */

#define PRO_RX_EXTEND		0007		/* extended function */
#define PRO_RX_FUNC		0160		/* function */
#define PRO_RX_DONE		0010		/* done bit */
#define PRO_RX_DRIVE		0004		/* drive select */
#define PRO_RX_DISK		0002		/* disk select */
#define PRO_RX_DISKNUM		0006		/* msb drive select, lsb disk select */

/* function codes */

#define PRO_RX_CMD_STAT		00		/* read status */
#define PRO_RX_CMD_MAINT	01		/* maintenance mode */
#define PRO_RX_CMD_RESTORE	02		/* restore drive */
#define PRO_RX_CMD_INIT		03		/* rx init */
#define PRO_RX_CMD_READ		04		/* read sector */
#define PRO_RX_CMD_EXTEND	05		/* extended function */
#define PRO_RX_CMD_ADDR		06		/* read address */
#define PRO_RX_CMD_WRITE	07		/* write sector */

/* extended function codes */

#define PRO_RX_CMD_RETRY	00		/* read with retries */
#define PRO_RX_CMD_DELETE	01		/* write with deleted data mark */
#define PRO_RX_CMD_RFORMAT	02		/* report format */
#define PRO_RX_CMD_SFORMAT	03		/* set format */
#define PRO_RX_CMD_VERSION	04		/* report version number */
#define PRO_RX_CMD_COMPARE	05		/* read and compare */

/* RD controller */

#define PRO_RD_OPENDED		0000001		/* operation ended */
#define PRO_RD_RESET		0000010		/* reset */
#define PRO_RD_DRQ		0000200		/* data transfer request */
#define PRO_RD_BUSY		0100000		/* busy */

#define PRO_RD_IDNF		0010000		/* ID not found error */
#define PRO_RD_ERRORS		0177400		/* mask for all errors */

#define PRO_RD_ERROR		0000400		/* error status */
#define PRO_RD_DATAREQ		0004000		/* data request */
#define PRO_RD_SEEKC		0010000		/* seek complete */
#define PRO_RD_WFAULT		0020000		/* write fault */
#define PRO_RD_DRDY		0040000		/* drive ready */

#define PRO_RD_SEC		0000377		/* sector mask */

/* RD commands */

#define PRO_RD_CMD		0377
#define PRO_RD_CMD_RESTORE	0020
#define PRO_RD_CMD_READ		0040
#define PRO_RD_CMD_WRITE	0060
#define PRO_RD_CMD_FORMAT	0120

/* LK201 Keyboard constants */

#define PRO_LK201_BELL_PITCH	2000		/* bell pitch in Hz */
#define PRO_LK201_BELL_DURATION	125		/* bell duration in ms */

/* Serial port assignments */

#define PRO_SER_NUMDEV		3

#define PRO_SER_KB		0
#define PRO_SER_PTR		1
#define PRO_SER_COM		2

/* Misc codes */

#define PRO_NOREG		0177777		/* response for undefined read */
#define PRO_NOCHAR		256		/* no character */
#define PRO_SUCCESS		1		/* success status code */
#define PRO_FAIL		-1		/* failure status code */

/* Video overlay */

#define PRO_OVERLAY_MAXA	128		/* max alpha value */
#define PRO_OVERLAY_A		80		/* alpha value for printing */

/* Serial I/O */

#define PRO_SERIAL_WDEPTH	2		/* serial port write FIFO depth */

/* Function Protoypes */

/* pro_init.c */

#include <stdio.h> /* For FILE */

#ifdef TRACE
GLOBAL int trace(void);
#endif
#ifdef IOTRACE
GLOBAL FILE *iotfptr;
#endif
GLOBAL struct sercall *pro_la50device;
GLOBAL struct sercall *pro_kb;
GLOBAL struct sercall *pro_ptr;
GLOBAL struct sercall *pro_com;
GLOBAL int pro_la50device_port;
GLOBAL int pro_kb_port;
GLOBAL int pro_ptr_port;
GLOBAL int pro_com_port;
GLOBAL int pro_exit_on_halt;
GLOBAL void pro_reset(void);
GLOBAL void pro_exit(void);

/* pro_2661kb.c */

GLOBAL void pro_2661kb_eq(void);
GLOBAL void pro_2661kb_poll_eq(void);
GLOBAL int pro_2661kb_rd(int pa);
GLOBAL void pro_2661kb_wr(int data, int pa, int access);
GLOBAL void pro_2661kb_reset(void);
GLOBAL void pro_2661kb_open(void);

/* pro_2661ptr.c */

GLOBAL void pro_2661ptr_eq(void);
GLOBAL void pro_2661ptr_poll_eq(void);
GLOBAL int pro_2661ptr_rd(int pa);
GLOBAL void pro_2661ptr_wr(int data, int pa, int access);
GLOBAL void pro_2661ptr_reset(void);
GLOBAL void pro_2661ptr_open(void);

/* pro_maint.c */

GLOBAL int pro_maint_mode;
GLOBAL int pro_maint_rd(int pa);
GLOBAL void pro_maint_wr(int data, int pa, int access);

/* pro_eq.c */

GLOBAL void pro_eq_sched(int eventnum, int interval);
GLOBAL int pro_eq_ptr;
GLOBAL unsigned char PRO_EQ[PRO_EQ_SIZE];
GLOBAL void (*pro_event[PRO_EQ_NUM_EVENTS+1])(void);
GLOBAL void pro_eq_reset(void);

/* pro_int.c */

GLOBAL int pro_int_irr[4];
GLOBAL void pro_int_set(int intnum);
GLOBAL int pro_int_ack(void);
GLOBAL int pro_int_rd(int pa);
GLOBAL void pro_int_wr(int data, int pa, int access);
GLOBAL void pro_int_reset(void);

/* pro_null.c */

GLOBAL struct sercall pro_null;
GLOBAL int pro_null_get(int dev);
GLOBAL int pro_null_put(int dev, int schar);
GLOBAL void pro_null_ctrl_get(int dev, struct serctrl *sctrl);
GLOBAL void pro_null_ctrl_put(int dev, struct serctrl *sctrl);
GLOBAL void pro_null_reset(int dev, int portnum);
GLOBAL void pro_null_exit(int dev);

/* pro_digipad.c */

GLOBAL struct sercall pro_digipad;
GLOBAL int pro_digipad_get(int dev);
GLOBAL int pro_digipad_put(int dev, int schar);
GLOBAL void pro_digipad_ctrl_get(int dev, struct serctrl *sctrl);
GLOBAL void pro_digipad_ctrl_put(int dev, struct serctrl *sctrl);
GLOBAL void pro_digipad_reset(int dev, int portnum);
GLOBAL void pro_digipad_exit(int dev);

/* pro_lk201.c */

GLOBAL struct sercall pro_lk201;
GLOBAL int pro_lk201_get(int dev);
GLOBAL int pro_lk201_put(int dev, int key);
GLOBAL void pro_lk201_ctrl_get(int dev, struct serctrl *sctrl);
GLOBAL void pro_lk201_ctrl_put(int dev, struct serctrl *sctrl);
GLOBAL void pro_lk201_reset(int dev, int portnum);
GLOBAL void pro_lk201_exit(int dev);

/* pro_la50.c */

GLOBAL int pro_la50_dpi;
GLOBAL struct sercall pro_la50;
GLOBAL int pro_la50_get(int dev);
GLOBAL int pro_la50_put(int dev, int key);
GLOBAL void pro_la50_ctrl_get(int dev, struct serctrl *sctrl);
GLOBAL void pro_la50_ctrl_put(int dev, struct serctrl *sctrl);
GLOBAL void pro_la50_reset(int dev, int portnum);
GLOBAL void pro_la50_exit(int dev);

/* pro_serial.c */

GLOBAL struct sercall pro_serial;
GLOBAL int pro_lp_workaround;
GLOBAL char *pro_serial_devname[4];
GLOBAL int pro_serial_get(int dev);
GLOBAL int pro_serial_put(int dev, int schar);
GLOBAL void pro_serial_ctrl_get(int dev, struct serctrl *sctrl);
GLOBAL void pro_serial_ctrl_put(int dev, struct serctrl *sctrl);
GLOBAL void pro_serial_reset(int dev, int portnum);
GLOBAL void pro_serial_exit(int dev);

/* pro_kb.c */

GLOBAL struct sercall *pro_kb_orig;
GLOBAL int pro_kb_get(void);
GLOBAL int pro_kb_rd(int pa);
GLOBAL void pro_kb_wr(int data, int pa, int access);
GLOBAL void pro_kb_filter(void);
GLOBAL void pro_kb_reset(void);
GLOBAL void pro_kb_open(void);
GLOBAL void pro_kb_exit(void);

/* pro_ptr.c */

GLOBAL int pro_ptr_rd(int pa);
GLOBAL void pro_ptr_wr(int data, int pa, int access);
GLOBAL void pro_ptr_reset(void);
GLOBAL void pro_ptr_open(void);
GLOBAL void pro_ptr_exit(void);

/* pro_clk.c */

GLOBAL int pro_force_year;
GLOBAL int pro_int_throttle;
GLOBAL void pro_clk_eq(void);
GLOBAL void pro_clk_uf_eq(void);
GLOBAL int pro_clk_rd(int pa);
GLOBAL void pro_clk_wr(int data, int pa, int access);
GLOBAL void pro_clk_reset(void);

/* pro_com.c */

GLOBAL void pro_com_eq(void);
GLOBAL void pro_com_poll_eq(void);
GLOBAL int pro_com_rd(int pa);
GLOBAL void pro_com_wr(int data, int pa, int access);
GLOBAL void pro_com_reset(void);
GLOBAL void pro_com_open(void);
GLOBAL void pro_com_exit(void);

/* pro_vid.c */

GLOBAL void pro_clear_mvalid(void);
GLOBAL unsigned short PRO_VRAM[3][PRO_VID_MEMSIZE/2];
GLOBAL unsigned char pro_vid_mvalid[(PRO_VID_MEMSIZE/2) >> PRO_VID_CLS];
GLOBAL int pro_vid_p1c;
GLOBAL int pro_vid_scl;
GLOBAL int pro_vid_csr;
GLOBAL int pro_vid_y_offset;

GLOBAL void pro_vid_eq(void);
GLOBAL void pro_vid_cmd_eq(void);
GLOBAL int pro_vram_rd(int *data, int pa, int access);
GLOBAL int pro_vram_wr(int data, int pa, int access);
GLOBAL int pro_vid_rd(int pa);
GLOBAL void pro_vid_wr(int data, int pa, int access);
GLOBAL void pro_vid_reset(void);
GLOBAL void pro_vid_exit(void);

/* pro_menu.c */

GLOBAL int pro_menu(int key);
GLOBAL void pro_menu_reset(void);

/* pro_rd.c */

GLOBAL char *pro_rd_dir;
GLOBAL char *pro_rd_file;
GLOBAL int pro_rd_heads;
GLOBAL int pro_rd_cyls;
GLOBAL int pro_rd_secs;
GLOBAL void pro_rd_eq(void);
GLOBAL int pro_rd_rd(int pa);
GLOBAL void pro_rd_wr(int data, int pa, int access);
GLOBAL void pro_rd_reset(void);
GLOBAL void pro_rd_exit(void);

/* pro_rx.c */

GLOBAL char *pro_rx_dir[4];
GLOBAL char *pro_rx_file[4];
GLOBAL int pro_rx_closed[4];
GLOBAL void pro_rx_open_door(int disknum);
GLOBAL void pro_rx_close_door(int disknum);
GLOBAL int pro_rx_rd(int pa);
GLOBAL void pro_rx_wr(int data, int pa, int access);
GLOBAL void pro_rx_reset(void);
GLOBAL void pro_rx_exit(void);

/* pro_mem.c */

GLOBAL unsigned short MEMROM[1024];
GLOBAL int pro_mem_rd(int pa);
GLOBAL void pro_mem_wr(int data, int pa, int access);
GLOBAL void pro_mem_reset(void);
GLOBAL void pro_mem_exit(void);

/* pro_reg.c */

GLOBAL int pro_led;
GLOBAL int pro_csr;
GLOBAL unsigned short ROM[8192];
GLOBAL unsigned short IDROM[32];
GLOBAL unsigned short BATRAM[50];
GLOBAL int rom_rd(int *data, int pa, int access);
GLOBAL int rom_wr(int data, int pa, int access);
GLOBAL int reg_rd(int *data, int pa, int access);
GLOBAL int reg_wr(int data, int pa, int access);

/* term_x11.c / term_ncurses.h */

GLOBAL int pro_nine_workaround;
GLOBAL int pro_libc_workaround;
GLOBAL int pro_window_x;
GLOBAL int pro_window_y;
GLOBAL int pro_screen_full;
GLOBAL int pro_screen_window_scale;
GLOBAL int pro_screen_full_scale;
GLOBAL int pro_screen_gamma;
GLOBAL int pro_screen_pcm;
GLOBAL int pro_screen_framebuffers;
GLOBAL int pro_mouse_x;
GLOBAL int pro_mouse_y;
GLOBAL int pro_mouse_l;
GLOBAL int pro_mouse_m;
GLOBAL int pro_mouse_r;
GLOBAL int pro_mouse_in;
GLOBAL int pro_keyboard_get(void);
GLOBAL void pro_keyboard_bell_vol(int vol);
GLOBAL void pro_keyboard_bell(void);
GLOBAL void pro_keyboard_auto_off(void);
GLOBAL void pro_keyboard_auto_on(void);
GLOBAL void pro_keyboard_click_off(void);
GLOBAL void pro_keyboard_click_on(void);

GLOBAL void pro_colormap_write(int index, int rgb);
GLOBAL void pro_mapchange(void);
GLOBAL void pro_scroll(void);
GLOBAL int pro_screen_init(void);
GLOBAL void pro_screen_update(void);
GLOBAL void pro_screen_close(void);
GLOBAL void pro_screen_reset(void);
GLOBAL void pro_screen_title(char *title);
GLOBAL void pro_screen_service_events(void);

/* term_overlay_x11.c / term_ncurses.h */

GLOBAL int pro_overlay_on;
GLOBAL unsigned char *pro_overlay_data;
GLOBAL unsigned char *pro_overlay_alpha;
GLOBAL void pro_overlay_print(int x, int y, int xnor, int font, char *text);
GLOBAL void pro_overlay_clear(void);
GLOBAL void pro_overlay_enable(void);
GLOBAL void pro_overlay_disable(void);
GLOBAL void pro_overlay_init(int psize, int cmode, int bpixel, int wpixel);
GLOBAL void pro_overlay_close(void);
