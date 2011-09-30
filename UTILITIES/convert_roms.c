/* convert_roms.c: convert pro350.rom and id.rom to C files, for compilation

   Copyright (c) 1997-2006, Tarik Isani (xhomer@isani.org)

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


#include <stdio.h>

void main()
{
FILE		*fptr1, *fptr2;
unsigned char	h, l;
int		i, j;
unsigned short	memval;


	printf("Converting pro350.rom to pro350.h\n");

	/* Load boot/diagnostic ROM */

	fptr1 = fopen("pro350.rom","r");

	if (fptr1 == NULL)
	{
	  printf("Unable to open pro350.rom for reading\n");
	  exit(0);
	}

	fptr2 = fopen("pro350.h","w");

	if (fptr2 == NULL)
	{
	  printf("Unable to open pro350.h for writing\n");
	  exit(0);
	}

	fprintf(fptr2, "/* pro350.h: PRO 350 boot ROM */\n\n");

	fprintf(fptr2, "unsigned short ROM[8192] =\n");
	fprintf(fptr2, "  {\n");

	for(i=0 ; i<1024; i++)
	{
	  fprintf(fptr2, "   ");
	  for(j=0 ; j<8; j++)
	  {
	    l = getc(fptr1);
	    h = getc(fptr1);
	    memval = h*256+l;
	    fprintf(fptr2, " 0x%04x", memval);
	    if ((i != 1023) || (j != 7))
	      fprintf(fptr2, ",");
	  }
	  fprintf(fptr2, "\n");
	}
	  
	fprintf(fptr2, "  };\n");

	fclose(fptr2);
	fclose(fptr1);


	printf("Converting id.rom to id.h\n");

	/* Load ID ROM */

	fptr1 = fopen("id.rom","r");

	if (fptr1 == NULL)
	{
	  printf("Unable to open id.rom for reading\n");
	  exit(0);
	}

	fptr2 = fopen("id.h","w");

	if (fptr2 == NULL)
	{
	  printf("Unable to open id.h for writing\n");
	  exit(0);
	}

	fprintf(fptr2, "/* id.h: PRO 350 ID ROM */\n\n");

	fprintf(fptr2, "unsigned short IDROM[32] =\n");
	fprintf(fptr2, "  {\n");

	for(i=0 ; i<4; i++)
	{
	  fprintf(fptr2, "   ");
	  for(j=0 ; j<8; j++)
	  {
	    l = getc(fptr1);
	    h = getc(fptr1);
	    memval = h*256+l;
	    fprintf(fptr2, " 0x%04x", memval);
	    if ((i != 3) || (j != 7))
	      fprintf(fptr2, ",");
	  }
	  fprintf(fptr2, "\n");
	}
	  
	fprintf(fptr2, "  };\n");

	fclose(fptr2);
	fclose(fptr1);
}
