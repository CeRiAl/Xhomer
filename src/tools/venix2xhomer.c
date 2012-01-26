/* venix2xhomer:

   Reads RX50 disk image files produced by Venix with
   "dd if=/dev/rf0 of=..." and converts them to the
   format required by Xhomer.


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


#include <string.h>
#include <stdio.h>

#define SECSIZE		512
#define SECTORS		10
#define TRACKS		79

int main(int argc, char *argv[])
{
const int	 map[10][10] = {
  {0, 2, 4, 6, 8, 1, 3, 5, 7, 9},
  {3, 5, 7, 9, 1, 4, 6, 8, 0, 2},
  {6, 8, 0, 2, 4, 7, 9, 1, 3, 5},
  {9, 1, 3, 5, 7, 0, 2, 4, 6, 8},
  {2, 4, 6, 8, 0, 3, 5, 7, 9, 1},
  {5, 7, 9, 1, 3, 6, 8, 0, 2, 4},
  {8, 0, 2, 4, 6, 9, 1, 3, 5, 7},
  {1, 3, 5, 7, 9, 2, 4, 6, 8, 0},
  {4, 6, 8, 0, 2, 5, 7, 9, 1, 3},
  {7, 9, 1, 3, 5, 8, 0, 2, 4, 6}
  };

int		track, venix_sector, xhomer_sector, c;

unsigned char	xhomer_rx50[TRACKS*SECTORS*SECSIZE];

FILE		*fptr_v;
FILE		*fptr_x;


if (argc != 3)
{
  printf("Usage: venix2xhomer venix.rx50 xhomer.rx50\n");
  exit(1);
}
else
{
  fptr_v = fopen(argv[1], "r");

  if (fptr_v == NULL)
  {
    printf("Unable to open %s\n", argv[1]);
    exit(1);
  }

  fptr_x = fopen(argv[2], "w");

  if (fptr_x == NULL)
  {
    printf("Unable to open %s\n", argv[2]);
    exit(1);
  }

  memset(&xhomer_rx50, 0, sizeof(xhomer_rx50));

  for(track = 0; track < TRACKS; track++)
    for(venix_sector = 0; venix_sector < SECTORS; venix_sector++)
    {
      xhomer_sector = map[track % 10][venix_sector];
/*
      printf("Translating track %d, sector %d to %d\n", track, venix_sector, xhomer_sector);
*/

      for(c = 0; c < SECSIZE; c++)
        xhomer_rx50[track*SECTORS*SECSIZE+xhomer_sector*SECSIZE+c] = getc(fptr_v);
    }	  

  /* The first track is missing from the Venix dumps */

  for(c = 0; c < SECTORS*SECSIZE; c++)
    putc(0, fptr_x);

  /* Write the remapped sectors */

  for(c = 0; c < TRACKS*SECTORS*SECSIZE; c++)
    putc(xhomer_rx50[c], fptr_x);

  fclose(fptr_x);
  fclose(fptr_v);
}
}
