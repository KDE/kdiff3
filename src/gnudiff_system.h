/* System dependent declarations.

   Modified for KDiff3 by Joachim Eibl 2003.
   The original file was part of GNU DIFF.

   Copyright (C) 1988, 1989, 1992, 1993, 1994, 1995, 1998, 2001, 2002
   Free Software Foundation, Inc.

   GNU DIFF is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU DIFF is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef GNUDIFF_SYSTEM_H
#define GNUDIFF_SYSTEM_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

/* Determine whether an integer type is signed, and its bounds.
   This code assumes two's (or one's!) complement with no holes.  */

/* The extra casts work around common compiler bugs,
   e.g. Cray C 5.0.3.0 when t == time_t.  */
#ifndef TYPE_SIGNED
# define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#endif
/* Verify a requirement at compile-time (unlike assert, which is runtime).  */
#define verify(name, assertion) struct name { char a[(assertion) ? 1 : -1]; }

#ifndef MIN
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif


/* The integer type of a line number. */

typedef size_t lin;
#define LIN_MAX PTRDIFF_MAX
verify (lin_is_wide_enough, sizeof (size_t) <= sizeof (lin));

#endif
