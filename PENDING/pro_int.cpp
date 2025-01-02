/* pro_int.cpp: interrupt controllers

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

/* XXX TBD:
		-only one routine can access intr_req at a time!
		-byte writes
		-clear highest isr command
		-clear int_req if pending interrupt goes away???
*/

#include "StdAfx.h"
#include "pdp11_defs.h"

extern int	int_req;		/* from pdp11_cpu */

int	pro_int_irr[3];			/* Interrupt request register (Регистр запроса обмена) */
int	pro_int_isr[3];			/* Interrupt service register (Регистр прерывания) */
int	pro_int_imr[3];			/* Interrupt mask register (Регистр запрета прерываний) */
int	pro_int_acr[3];			/* Auto clear register (Регистр автоочистки) */
int	pro_int_mode[3];		/* Mode register (write-only) (Регистр режима работы) */
int	pro_int_index[3];		/* Index in rotating mode */
int	pro_int_presel[3];		/* Write preselect */
int	pro_int_presel_vec[3];	/* Response memory preselected vector location */
int	pro_int_vec[3][8];		/* Interrupt vectors */


/* Determine highest priority int */
int pro_int_check (int cnum)
{
int irr = pro_int_irr[cnum];
int imr = pro_int_imr[cnum];
int	i, intreqs;

	intreqs = irr & ((~imr) & 0377);
	if (intreqs == 0)		/* no int pending */
		i = PRO_NO_INT;
	else
	{
		/* Determine highest pending interrupt (определяем линию (разряд) с наивысшим приоритетом, по которой поступил запрос обмена) */
		if ((pro_int_mode[cnum] & PRO_INT_PM) == 0)
			// fixed priority mode - Фиксированный приоритет. Линия нулевого разряда IRQ 00 обладает высшим приоритетом, низшим - седьмой разряд IRQ 07.
			for (i = 0; i <= 7; i++)
			{
				if ((intreqs & 01) == 1)
					break;
				intreqs = intreqs >> 1;
			}
		else
		{
			// rotating priority mode - Изменяющийся приоритет. Изменяется, по мере обслуживания прерываний, по кольцу 00..07..00.
			// Последняя обслуженная линия приобретает низший приоритет, на 1 больше - высший приоритет.
			for (i = pro_int_index[cnum]; i <= 7; i++)
			{
				intreqs = (irr & ((~imr) & 0377)) >> i;
				if ((intreqs & 01) == 1)
					return i;
			}
			if (pro_int_index[cnum] > 0)	// кольцуем начало
				for (i = 0; i < pro_int_index[cnum]; i++)
				{
					intreqs = (irr & ((~imr) & 0377)) >> i;
					if ((intreqs & 01) == 1)
						break;
				}
		}
	}
	return i;
}


/* Update status of int line to processor */
void pro_int_update (void)
{
int	intset, cnum, intnum;

	intset = 0;

	for (cnum = 0; cnum <= 2; cnum++)
	{
		intnum = pro_int_check(cnum);
		if ((intnum != PRO_NO_INT) && ((pro_int_mode[cnum] & PRO_INT_MM) != 0) && ((pro_int_mode[cnum] & PRO_INT_IM) == 0))
			intset = 1;
	}

	/* Set or clear interrupt, as needed */
	if (intset == 1)
		int_req = int_req | INT_PIR4;
	else
		int_req = int_req & ~INT_PIR4;
}


int pro_int_ack (void)
{
/* XXX handle case where no interrupt is set */
int	cnum, intnum, vec;

	for (cnum = 0; cnum <= 2; cnum++)
	{
		intnum = pro_int_check(cnum);
		if ((intnum != PRO_NO_INT) && ((pro_int_mode[cnum] & PRO_INT_MM) != 0) && ((pro_int_mode[cnum] & PRO_INT_IM) == 0))
			break;
	}

	/* Lookup interrupt vector to return */
	if ((pro_int_mode[cnum] & PRO_INT_VS) != 0)
		vec = pro_int_vec[cnum][0];	/* return common vector if VSS set */
	else
		vec = pro_int_vec[cnum][intnum];

	/* Clear IRR bit */
	pro_clrbit(pro_int_irr[cnum], intnum);

	/* Set ISR bit if corresponding ACR bit is cleared */
	if (((pro_int_acr[cnum] >> intnum) & 01) == 0)
		pro_setbit(pro_int_isr[cnum], intnum);

	if ((pro_int_mode[cnum] & PRO_INT_PM) != 0)
	{	// rotating priority mode - Изменяющийся приоритет. Изменяется, по мере обслуживания прерываний, по кольцу 00..07..00.
		pro_int_index[cnum]++; // new higest priority interrupt bit is next after the last interrupt serviced
		if (pro_int_index[cnum] > 7)
			pro_int_index[cnum] = 0;
	}

	/* Check if more interrupts are pending, set J11 int bit again, if so */
	pro_int_update();

	return vec;
}


void pro_int_set (int intnum)
{
int	cnum, intbit;

	/* Extract controller number and interrupt bit */
	cnum = (intnum >> 8) & 0377;
	intbit = intnum & 0377;

	/* Set interrupt request */
	/* XXX Does master mask mask this? if (pro_int_mode[cnum] & PRO_INT_MM)... */
	pro_int_irr[cnum] |= intbit;

	pro_int_update();
}


/* Interrupt controller registers */
/* Addresses 17773200-17773212 */
int pro_int_rd (int pa)
{
int	data, i, cnum;

	/* Determine interrupt controller number */
	cnum = (pa >> 2) & 03; // 0, 1 or 2
	data = 0;
	switch (pa & 02)
	{
		case 00:	/* read - Data registers: 17773200 (0) or 17773204 (1) or 17773210 (2) (регистры данных) */
			switch ((pro_int_mode[cnum] >> 5) & 03) // 'data' transferred to the processor specified by the <06><05> RP1-RP0 - Register Preselect
			{
				case 00:
					data = pro_int_isr[cnum];
					break;
				case 01:
					data = pro_int_imr[cnum];
					break;
				case 02:
					data = pro_int_irr[cnum];
					break;
				case 03:
					data = pro_int_acr[cnum];
					break;
			}
			break;
		case 02:	/* read - CSR registers: 17773202 (0), 17773206 (1), 17773212 (2) (регистры статуса при чтении) */
			i = pro_int_check(cnum);

			if (i == PRO_NO_INT)	/// XXX check if PRO_INT_IM mode!
				data = 0200;		/* no int pending, return 0 in lsbs? (в РЗО нет установленных разрядов незаблокированных в РЗП. Разряд 7 Используется в флаговом режиме) */
			else
				data = i;			/* int pending (разряды: <02><01><00> (при <07>==0)) */
			data = data | ((pro_int_mode[cnum] & PRO_INT_MM) >> 4)	// <07> -> <03> Генерация требований прерывания: 0 - запрещена, 1 - прерывания разрешены
						| ((pro_int_mode[cnum] & PRO_INT_IM) << 2)	// <02> -> <04> 0 - режим прерываний, 1 - флаговый режим
						| ((pro_int_mode[cnum] & PRO_INT_PM) << 5);	// <00> -> <05> Приоритет: 0 - фиксированный (линия нулевого разряда - высший, седьмого - низший), 1 - изменяется по мере обслуживания прерываний
			break;
		default:
			break;
	}
	return data;
}

void pro_int_wr (int data, int pa, int access)
{
int	cnum;

	/* XXX byte accesses not handled correctly yet (разряды 8-15 при считывании - 0, запись эффекта не имеет) */
	/* Determine interrupt controller number */
	cnum = (pa >> 2) & 03;

	switch (pa & 02)
	{
		case 00:	/* write - Data registers: 17773200 (0), 17773204 (1), 17773210 (2) (регистры данных) */
			switch (pro_int_presel[cnum]) // 'data' transferred to the intrnal register specified by the last 'preselect command'
			{
				case PRO_PRESEL_IMR:
					pro_int_imr[cnum] = data;
					pro_int_update();
					break;
				case PRO_PRESEL_ACR:
					pro_int_acr[cnum] = data;
					break;
				case PRO_PRESEL_RESP:
					pro_int_vec[cnum][pro_int_presel_vec[cnum]] = data;
					break;
				default:
					break;
			}
			break;
		case 02:	/* write - CSR registers: 17773202 (0), 17773206 (1), 17773212 (2) (регистры команд при записи) */
			if ((data & 0377) == 0)	/* reset */
			{
				pro_int_irr[cnum] = 0;
				pro_int_isr[cnum] = 0;
				pro_int_imr[cnum] = 0377;
				pro_int_acr[cnum] = 0;
				pro_int_mode[cnum] = 0;
				pro_int_presel[cnum] = 0;
				pro_int_update();
				break;
			}
			else
				switch (data & 0370) /* 11111000 */
				{
					case 0020:		/* clear irr and imr */
						pro_int_irr[cnum] = 0; // РЗО
						pro_int_imr[cnum] = 0; // РЗП
						pro_int_update();
						break;
					case 0030:		/* clear single irr and imr bit */
						pro_clrbit(pro_int_irr[cnum], (data & 07));
						pro_clrbit(pro_int_imr[cnum], (data & 07));
						pro_int_update();
						break;
					case 0040:		/* clear imr */
						pro_int_imr[cnum] = 0;
						pro_int_update();
						break;
					case 0050:		/* clear single imr bit */
						pro_clrbit(pro_int_imr[cnum], (data & 07));
						pro_int_update();
						break;
					case 0060:		/* set imr */
						pro_int_imr[cnum] = 0377;
						pro_int_update();
						break;
					case 0070:		/* set single imr bit */
						pro_setbit(pro_int_imr[cnum], (data & 07));
						pro_int_update();
						break;
					case 0100:		/* clear irr */
						pro_int_irr[cnum] = 0;
						pro_int_update();
						break;
					case 0110:		/* clear single irr bit */
						pro_clrbit(pro_int_irr[cnum], (data & 07));
						pro_int_update();
						break;
					case 0120:		/* set irr */
						pro_int_irr[cnum] = 0377;
						pro_int_update();
						break;
					case 0130:		/* set single irr bit */
						pro_setbit(pro_int_irr[cnum], (data & 07));
						pro_int_update();
						break;
					case 0140:		/* clear highest priority isr bit */
					case 0150:		/* highest priority bit in isr is cleared */
						/// Производит запись 0 в РП по разряду с высшим приоритетом
						/* XXX unimplemented */
						pro_int_update();
						break;
					case 0160:		/* clear isr */
						pro_int_isr[cnum] = 0;
						break;
					case 0170:		/* clear single isr bit */
						pro_clrbit(pro_int_isr[cnum], (data & 07));
						break;
					case 0200:		/* load mode bits m0 thru m4 */
					case 0210:
					case 0220:
					case 0230:
						pro_int_mode[cnum] = (pro_int_mode[cnum] & 0340) | (data & 0037);
						pro_int_update();
						break;
					case 0240:		/* control mode bits m5 thru m7 and operate group interrupts */
					case 0250:
						pro_int_mode[cnum] = (pro_int_mode[cnum] & 0237) | ((data << 3) & 0140);
						if ((data & 03) == 01)
							pro_setbit(pro_int_mode[cnum], 7); // Enables group interrupts to the processor
						else if ((data & 03) == 02)
							pro_clrbit(pro_int_mode[cnum], 7); // Disables group interrupts to the processor
						pro_int_update();
						break;
					case 0260:		/* preselect imr for writing (Выбор РЗП) */
					case 0270:
						pro_int_presel[cnum] = PRO_PRESEL_IMR;
						break;
					case 0300:		/* preselect acr for writing (Выбор РА) */
					case 0310:
						pro_int_presel[cnum] = PRO_PRESEL_ACR;
						break;
					case 0340:		/* preselect response memory for writing (выбрано ОЗУ векторов прерываний) */
						pro_int_presel[cnum] = PRO_PRESEL_RESP;
						pro_int_presel_vec[cnum] = (data & 07); // for above commands used 'single' YYY bits, the bits specified is as: 0 - LSB ... 7 - MSB
						break;
					default:
						break;
				}
			break;
		default:
			break;
	}
}


void pro_int_reset ()
{
int cnum, intnum;

	for (cnum = 0; cnum <= 2; cnum++)
	{
		pro_int_irr[cnum] = 0;
		pro_int_isr[cnum] = 0;
		pro_int_imr[cnum] = 0377;
		pro_int_acr[cnum] = 0;
		pro_int_mode[cnum] = 0;
		pro_int_index[cnum] = 0;
		pro_int_presel[cnum] = 0;
		pro_int_presel_vec[cnum] = 0;
		for (intnum = 0; intnum <= 7; intnum++)
			pro_int_vec[cnum][intnum] = 0;
	}
	
	/* Probably not necessary */
	pro_int_update();
}
