/* System dependent declarations.

   Modified for KDiff3 by Joachim Eibl <joachim.eibl at gmx.de> 2003.
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
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef GNUDIFF_SYSTEM_H
#define GNUDIFF_SYSTEM_H

#include <QtGlobal>

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

/* The integer type of a line number. */

typedef int LineRef;
#define LINEREF_MAX INT_MAX

static_assert(std::is_signed<LineRef>::value, "LineRef must be signed.");
//verify(lin_is_wide_enough, sizeof(int) <= sizeof(LineRef));

#endif
