/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996-1998 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: copyout.c,v 1.9 1998/04/11 08:26:54 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

#include <stdio.h>

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# else
int open(const char *, int, ...);
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# else
int dup(int);
# endif

# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <sys/stat.h>

# include "libhfs//hfs.h"
# include "libhfs/apple.h"
# include "libhfs/data.h"
# include "copyout.h"
# include "charset.h"
# include "binhex.h"
# include "crc.h"

const char *cpo_error = "no error";

extern int errno;

# define ERROR(code, str)	(cpo_error = (str), errno = (code))

# define MACB_BLOCKSZ	128

/* Copy Routines =========================================================== */
/*
 * NAME:	do_macb()
 * DESCRIPTION:	perform copy using MacBinary II translation
 */
int do_macb(hfsfile *ifile, void *outputbuf)
{
  hfsdirent ent;
  unsigned char buf[MACB_BLOCKSZ];
  void *pos = outputbuf;

  if (hfs_fstat(ifile, &ent) == -1)
    {
      ERROR(errno, hfs_error);
      return -1;
    }

  memset(buf, 0, MACB_BLOCKSZ);

  buf[1] = strlen(ent.name);
  strcpy((char *) &buf[2], ent.name);

  memcpy(&buf[65], ent.u.file.type,    4);
  memcpy(&buf[69], ent.u.file.creator, 4);

  buf[73] = ent.fdflags >> 8;

  d_putul(&buf[83], ent.u.file.dsize);
  d_putul(&buf[87], ent.u.file.rsize);

  d_putul(&buf[91], d_mtime(ent.crdate));
  d_putul(&buf[95], d_mtime(ent.mddate));

  buf[101] = ent.fdflags & 0xff;
  buf[122] = buf[123] = 129;

  d_putuw(&buf[124], crc_macb(buf, 124, 0x0000));

  memcpy(pos,buf,MACB_BLOCKSZ);
  pos += MACB_BLOCKSZ;
  fprintf(stderr,"header: %d\n",MACB_BLOCKSZ);
  if (hfs_setfork(ifile, 0) == -1)
    {
      ERROR(errno, hfs_error);
      return -1;
    }

  int chunk = hfs_read(ifile, pos, ent.u.file.dsize);
  if (chunk == -1)
	{
	  ERROR(errno, hfs_error);
	  return -1;
	}
  pos+=ent.u.file.dsize;
  //fprintf(stderr,"data fork: %ld\n",ent.u.file.dsize);

  if (hfs_setfork(ifile, 1) == -1)
    {
      ERROR(errno, hfs_error);
      return -1;
    }

  chunk = hfs_read(ifile, pos, ent.u.file.rsize);
  if (chunk == -1)
	{
	  ERROR(errno, hfs_error);
	  return -1;
	}
  //fprintf(stderr,"rsrc fork: %ld\n",ent.u.file.rsize);

  return 0;
}

