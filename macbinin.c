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
 * $Id: copyin.c,v 1.8 1998/11/02 22:08:25 rob Exp $
 */
#include <stdio.h>
# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

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

# include "libhfs/hfs.h"
# include "copyin.h"
# include "charset.h"
# include "binhex.h"
# include "crc.h"
# include "libhfs/apple.h"
# include "libhfs/data.h"

const char *cpi_error = "no error";

extern int errno;

# define ERROR(code, str)	(cpi_error = (str), errno = (code))
//# define ERROR(code, str)	fprintf(stderr,"cpi error: %d %s\n",(code),(str))

# define MACB_BLOCKSZ	128

# define TEXT_TYPE	"TEXT"
# define TEXT_CREA	"UNIX"

# define RAW_TYPE	"????"
# define RAW_CREA	"UNIX"


/*
 * NAME:	opendst()
 * DESCRIPTION:	open the destination file
 */
static
hfsfile *opendst(hfsvol *vol, const char *dstname, const char *hint,
		 const char *type, const char *creator)
{
  hfsdirent ent;
  hfsfile *file;
  unsigned long cwd;

  if (hfs_stat(vol, dstname, &ent) != -1 &&
      (ent.flags & HFS_ISDIR))
    {
      cwd = hfs_getcwd(vol);

      if (hfs_setcwd(vol, ent.cnid) == -1)
	{
	  ERROR(errno, hfs_error);
	  return 0;
	}

      dstname = hint;
    }

  hfs_delete(vol, dstname);

  file = hfs_create(vol, dstname, type, creator);
  if (file == 0)
    {
      ERROR(errno, hfs_error);

      if (dstname == hint)
	hfs_setcwd(vol, cwd);

      return 0;
    }

  if (dstname == hint)
    {
      if (hfs_setcwd(vol, cwd) == -1)
	{
	  ERROR(errno, hfs_error);

	  hfs_close(file);
	  return 0;
	}
    }

  return file;
}


/* Interface Routines ====================================================== */

/*
 * NAME:	cpi->macb()
 * DESCRIPTION:	copy a UNIX file to an HFS file using MacBinary II translation
 */
int cpi_macb_data(hfsvol *vol, const char *dstname, const char *data)
{
  int  result = 0;
  hfsfile *ofile;
  hfsdirent ent;
  const char *dsthint;
  char type[5], creator[5];
  unsigned char buf[MACB_BLOCKSZ];
  unsigned short crc;
  unsigned long dsize, rsize;
  const void *ptr = data;
  int bytes;
  memcpy(buf,ptr,MACB_BLOCKSZ);
  ptr+=MACB_BLOCKSZ;


  if (buf[0] != 0 || buf[74] != 0)
    {
      ERROR(EINVAL, "invalid MacBinary file header");

      return -1;
    }

  crc = d_getuw(&buf[124]);

  if (crc_macb(buf, 124, 0x0000) != crc)
    {
      /* (buf[82] == 0) => MacBinary I? */

      ERROR(EINVAL, "unknown, unsupported, or corrupt MacBinary file");

      return -1;
    }

  if (buf[123] > 129)
    {
      ERROR(EINVAL, "unsupported MacBinary file version");

      return -1;
    }

  if (buf[1] < 1 || buf[1] > 63 ||
      buf[2 + buf[1]] != 0)
    {
      ERROR(EINVAL, "invalid MacBinary file header (bad file name)");

      return -1;
    }

  dsize = d_getul(&buf[83]);
  rsize = d_getul(&buf[87]);

  if (dsize > 0x7fffffff || rsize > 0x7fffffff)
    {
      ERROR(EINVAL, "invalid MacBinary file header (bad file length)");

      return -1;
    }

  dsthint = (char *) &buf[2];

  memcpy(type,    &buf[65], 4);
  memcpy(creator, &buf[69], 4);
  type[4] = creator[4] = 0;

  ofile = opendst(vol, dstname, dsthint, type, creator);
  if (ofile == 0)
    {
      fprintf(stderr,"opendst failed\n");
      return -1;
    }

  result = 0;
  if (hfs_setfork(ofile, 0) == -1)
    {
      ERROR(errno, hfs_error);
      return -1;
    }
  bytes = hfs_write(ofile, ptr, dsize);
  ptr+=dsize;
  if (bytes == -1)
  {
	  ERROR(errno, hfs_error);
	  return -1;
  }
  else if (bytes != dsize)
  {
	 ERROR(EIO, "wrote incomplete chunk");
	  return -1;
  }
  if (hfs_setfork(ofile, 1) == -1)
    {
      ERROR(errno, hfs_error);
      return -1;
    }
  bytes = hfs_write(ofile, ptr, rsize);
  ptr+=dsize;
  if (bytes == -1)
  {
	  ERROR(errno, hfs_error);
	  return -1;
  }
  else if (bytes != rsize)
  {
	 ERROR(EIO, "wrote incomplete chunk");
	  return -1;
  }

  if (result == 0 && hfs_fstat(ofile, &ent) == -1)
    {
      ERROR(errno, hfs_error);
      result = -1;
    }

  ent.fdflags = (buf[73] << 8 | buf[101]) &
    ~(HFS_FNDR_ISONDESK | HFS_FNDR_HASBEENINITED | HFS_FNDR_RESERVED);

  ent.crdate = d_ltime(d_getul(&buf[91]));
  ent.mddate = d_ltime(d_getul(&buf[95]));

  if (result == 0 && hfs_fsetattr(ofile, &ent) == -1)
    {
      ERROR(errno, hfs_error);
      result = -1;
    }

  if (ofile && hfs_close(ofile) == -1)
    {
      ERROR(errno, hfs_error);
      result = -1;
    }


  return result;
}

