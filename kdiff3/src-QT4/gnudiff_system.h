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



/* Don't bother to support K&R C compilers any more; it's not worth
   the trouble.  These macros prevent some library modules from being
   compiled in K&R C mode.  */
#define PARAMS(Args) Args
#define PROTOTYPES 1

/* Verify a requirement at compile-time (unlike assert, which is runtime).  */
#define verify(name, assertion) struct name { char a[(assertion) ? 1 : -1]; }


/* Determine whether an integer type is signed, and its bounds.
   This code assumes two's (or one's!) complement with no holes.  */

/* The extra casts work around common compiler bugs,
   e.g. Cray C 5.0.3.0 when t == time_t.  */
#ifndef TYPE_SIGNED
# define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#endif
#ifndef TYPE_MINIMUM
# define TYPE_MINIMUM(t) ((t) (TYPE_SIGNED (t) \
			       ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) \
			       : (t) 0))
#endif
#ifndef TYPE_MAXIMUM
# define TYPE_MAXIMUM(t) ((t) (~ (t) 0 - TYPE_MINIMUM (t)))
#endif

#include <sys/types.h>
#include <sys/stat.h>


# include <stdlib.h>
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#if !EXIT_FAILURE
# undef EXIT_FAILURE /* Sony NEWS-OS 4.0C defines EXIT_FAILURE to 0.  */
# define EXIT_FAILURE 1
#endif
#define EXIT_TROUBLE 2

#include <limits.h>
#ifndef SSIZE_MAX
# define SSIZE_MAX TYPE_MAXIMUM (ssize_t)
#endif

#ifndef PTRDIFF_MAX
# define PTRDIFF_MAX TYPE_MAXIMUM (ptrdiff_t)
#endif
#ifndef SIZE_MAX
# define SIZE_MAX TYPE_MAXIMUM (size_t)
#endif
#ifndef UINTMAX_MAX
# define UINTMAX_MAX TYPE_MAXIMUM (uintmax_t)
#endif

#include <stddef.h>
#include <string.h>
#include <ctype.h>

/* CTYPE_DOMAIN (C) is nonzero if the unsigned char C can safely be given
   as an argument to <ctype.h> macros like `isspace'.  */
# define CTYPE_DOMAIN(c) 1
#define ISPRINT(c) (CTYPE_DOMAIN (c) && isprint (c))
#define ISSPACE(c) (CTYPE_DOMAIN (c) && isspace (c))

# define TOLOWER(c) tolower (c)

/* ISDIGIT differs from isdigit, as follows:
   - Its arg may be any int or unsigned int; it need not be an unsigned char.
   - It's guaranteed to evaluate its argument exactly once.
   - It's typically faster.
   POSIX 1003.1-2001 says that only '0' through '9' are digits.
   Prefer ISDIGIT to isdigit unless it's important to use the locale's
   definition of `digit' even when the host does not conform to POSIX.  */
#define ISDIGIT(c) ((unsigned int) (c) - '0' <= 9)

#undef MIN
#undef MAX
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))


/* The integer type of a line number.  Since files are read into main
   memory, ptrdiff_t should be wide enough.  */

typedef ptrdiff_t lin;
#define LIN_MAX PTRDIFF_MAX
verify (lin_is_signed, TYPE_SIGNED (lin));
verify (lin_is_wide_enough, sizeof (ptrdiff_t) <= sizeof (lin));
//verify (lin_is_printable_as_long, sizeof (lin) <= sizeof (long));

#endif
