#include <stdio.h>

/* rx2f.c: Read raw image from RX50 device and extract single file

   Copyright (c) 1997-2024, Tarik Isani (xhomer@isani.org)

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


main()
{
  FILE	*fptr1;
  FILE	*fptr2;

  long	i, fs, fsm;

  char	c;

  char	fname[256];

  fptr1 = fopen("/dev/r50", "r");

  if (fptr1 == NULL)
  {
    printf("Unable to open /dev/r50\n");
    exit(1);
  }

  /* fs contains file size in bytes */

  fs = 0;

  fsm = 1000000000;

  for(i=0; i<10; i++)
  {
    c = getc(fptr1);
    fs = fs + ((long)c - 48) * fsm;
    fsm = fsm / 10;
  }

  /* skip separator */

  getc(fptr1);

  /* get file name */

  i = 0;

  c = getc(fptr1);

  while(c != 0xa)
  {
    fname[i++] = c;

    c = getc(fptr1);
  }

  fname[i] = 0;

  printf("Extracting %s (%ld bytes)\n", fname, fs);

  /* open output file */

  fptr2 = fopen(fname, "w");

  if (fptr2 == NULL)
  {
    printf("Unable to open %s\n", fname);
    exit(1);
  }

  for(i=0; i<fs; i++)
  {
    c = getc(fptr1);
    putc(c, fptr2);
  }

  fclose(fptr2);
  fclose(fptr1);
}
